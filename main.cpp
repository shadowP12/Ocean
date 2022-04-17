#include "OceanDefine.h"
#include "WavesGenerator.h"

#include <Blast/Gfx/GfxDefine.h>
#include <Blast/Gfx/GfxDevice.h>
#include <Blast/Gfx/Vulkan/VulkanDevice.h>
#include <Blast/Utility/ShaderCompiler.h>
#include <Blast/Utility/VulkanShaderCompiler.h>

#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm.hpp>
#include <gtx/quaternion.hpp>
#include <gtc/matrix_transform.hpp>

#include <iostream>
#include <fstream>
#include <sstream>

static std::string ProjectDir(PROJECT_DIR);

static std::string ReadFileData(const std::string& path) {
    std::istream* stream = &std::cin;
    std::ifstream file;

    file.open(path, std::ios_base::binary);
    stream = &file;
    if (file.fail()) {
        BLAST_LOGW("cannot open input file %s \n", path.c_str());
        return std::string("");
    }
    return std::string((std::istreambuf_iterator<char>(*stream)), std::istreambuf_iterator<char>());
}

static std::pair<blast::GfxShader*, blast::GfxShader*> CompileShaderProgram(const std::string& vs_path, const std::string& fs_path);

static blast::GfxShader* CompileComputeShader(const std::string& cs_path);

static void RefreshSwapchain(void* window, uint32_t width, uint32_t height);

static void CursorPositionCallback(GLFWwindow* window, double pos_x, double pos_y);

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods);

static void MouseScrollCallback(GLFWwindow* window, double offset_x, double offset_y);

blast::ShaderCompiler* g_shader_compiler = nullptr;
blast::GfxDevice* g_device = nullptr;
blast::GfxSwapChain* g_swapchain = nullptr;

blast::GfxTexture* scene_color_tex = nullptr;
blast::GfxTexture* scene_depth_tex = nullptr;
blast::GfxTexture* resolve_tex = nullptr;
blast::GfxShader* scene_vert_shader = nullptr;
blast::GfxShader* scene_frag_shader = nullptr;
blast::GfxRenderPass* scene_renderpass = nullptr;
blast::GfxPipeline* scene_pipeline = nullptr;

blast::GfxShader* blit_vert_shader = nullptr;
blast::GfxShader* blit_frag_shader = nullptr;
blast::GfxPipeline* blit_pipeline = nullptr;

blast::GfxShader* copy_shader = nullptr;
blast::GfxShader* luminance_shader = nullptr;
blast::GfxShader* fft_shader = nullptr;

blast::GfxBuffer* g_quad_index_buffer = nullptr;
blast::GfxBuffer* g_quad_vertex_buffer = nullptr;

blast::GfxSampler* linear_sampler = nullptr;
blast::GfxSampler* nearest_sampler = nullptr;

blast::GfxTexture* test_texture = nullptr;
blast::GfxTexture* luminance_texture = nullptr;
blast::GfxTexture* result_texture = nullptr;

blast::GfxBuffer* object_ub = nullptr;

blast::SampleCount g_sample_count = blast::SAMPLE_COUNT_4;

WavesGenerator* waves_generator = nullptr;

Context* g_context = nullptr;

struct ObjectUniforms {
    glm::mat4 model_matrix;
    glm::mat4 view_matrix;
    glm::mat4 proj_matrix;
};

struct Camera {
    glm::vec3 position = glm::vec3(0.0f, 0.0f, 0.0f);
    glm::vec3 front = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec2 start_point = glm::vec2(-1.0f, -1.0f);
    float yaw = -90.0f;
    float pitch = 0.0f;
    bool grabbing = false;
} camera;

float quad_vertices[] = {
        -1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f,
        1.0f, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f,
        1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f,
        -1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f
};

unsigned int quad_indices[] = {
        0, 1, 2, 2, 3, 0
};

int main() {
    g_shader_compiler = new blast::VulkanShaderCompiler();

    g_device = new blast::VulkanDevice();

    // load shader
    {
        auto shaders = CompileShaderProgram(ProjectDir + "/Resources/Shaders/blit.vert", ProjectDir + "/Resources/Shaders/blit.frag");
        blit_vert_shader = shaders.first;
        blit_frag_shader = shaders.second;
    }
    {
        auto shaders = CompileShaderProgram(ProjectDir + "/Resources/Shaders/scene.vert", ProjectDir + "/Resources/Shaders/scene.frag");
        scene_vert_shader = shaders.first;
        scene_frag_shader = shaders.second;
    }
    {
        copy_shader = CompileComputeShader(ProjectDir + "/Resources/Shaders/copy.comp");
    }
    {
        fft_shader = CompileComputeShader(ProjectDir + "/Resources/Shaders/fft.comp");
    }
    {
        luminance_shader = CompileComputeShader(ProjectDir + "/Resources/Shaders/luminance.comp");
    }

    blast::GfxCommandBuffer* copy_cmd = g_device->RequestCommandBuffer(blast::QUEUE_COPY);

    g_context = new Context;
    g_context->device = g_device;
    g_context->fft_shader = fft_shader;
    g_context->copy_shader = copy_shader;

    // load quad buffers
    {
        blast::GfxBufferDesc buffer_desc;
        buffer_desc.size = sizeof(Vertex) * 4;
        buffer_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
        buffer_desc.res_usage = blast::RESOURCE_USAGE_VERTEX_BUFFER;
        g_quad_vertex_buffer = g_device->CreateBuffer(buffer_desc);
        {
            g_device->UpdateBuffer(copy_cmd, g_quad_vertex_buffer, quad_vertices, sizeof(Vertex) * 4);
            blast::GfxBufferBarrier barrier;
            barrier.buffer = g_quad_vertex_buffer;
            barrier.new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            g_device->SetBarrier(copy_cmd, 1, &barrier, 0, nullptr);
        }

        buffer_desc.size = sizeof(unsigned int) * 6;
        buffer_desc.mem_usage = blast::MEMORY_USAGE_CPU_TO_GPU;
        buffer_desc.res_usage = blast::RESOURCE_USAGE_INDEX_BUFFER;
        g_quad_index_buffer = g_device->CreateBuffer(buffer_desc);
        {
            g_device->UpdateBuffer(copy_cmd, g_quad_index_buffer, quad_indices, sizeof(unsigned int) * 6);
            blast::GfxBufferBarrier barrier;
            barrier.buffer = g_quad_index_buffer;
            barrier.new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            g_device->SetBarrier(copy_cmd, 1, &barrier, 0, nullptr);
        }
    }

    blast::GfxSamplerDesc sampler_desc = {};
    linear_sampler = g_device->CreateSampler(sampler_desc);
    sampler_desc.min_filter = blast::FILTER_NEAREST;
    sampler_desc.mag_filter = blast::FILTER_NEAREST;
    nearest_sampler = g_device->CreateSampler(sampler_desc);

    {
        int tex_width, tex_height, tex_channels;
        std::string image_path = ProjectDir + "/Resources/Textures/test.png";
        unsigned char* pixels = stbi_load(image_path.c_str(), &tex_width, &tex_height, &tex_channels, STBI_rgb_alpha);

        blast::GfxTextureDesc texture_desc;
        texture_desc.width = tex_width;
        texture_desc.height = tex_height;
        texture_desc.format = blast::FORMAT_R8G8B8A8_UNORM;
        texture_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
        texture_desc.res_usage = blast::RESOURCE_USAGE_SHADER_RESOURCE | blast::RESOURCE_USAGE_UNORDERED_ACCESS;
        test_texture = g_device->CreateTexture(texture_desc);
        {
            blast::GfxTextureBarrier barrier;
            barrier.texture = test_texture;
            barrier.new_state = blast::RESOURCE_STATE_COPY_DEST;
            g_device->SetBarrier(copy_cmd, 0, nullptr, 1, &barrier);

            g_device->UpdateTexture(copy_cmd, test_texture, pixels);

            barrier.texture = test_texture;
            barrier.new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            g_device->SetBarrier(copy_cmd, 0, nullptr, 1, &barrier);
        }
        stbi_image_free(pixels);

        texture_desc.format = blast::FORMAT_R32G32_FLOAT;
        texture_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
        texture_desc.res_usage = blast::RESOURCE_USAGE_SHADER_RESOURCE | blast::RESOURCE_USAGE_UNORDERED_ACCESS;
        result_texture = g_device->CreateTexture(texture_desc);
        luminance_texture = g_device->CreateTexture(texture_desc);
    }

    std::vector<ObjectUniforms> object_storages(2);
    {
        blast::GfxBufferDesc buffer_desc = {};
        buffer_desc.size = sizeof(ObjectUniforms) * object_storages.size();
        buffer_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
        buffer_desc.res_usage = blast::RESOURCE_USAGE_UNIFORM_BUFFER;
        object_ub = g_device->CreateBuffer(buffer_desc);
    }

    waves_generator = new WavesGenerator(g_context, 512, 512);

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(512, 512, "Ocean", nullptr, nullptr);
    glfwSetCursorPosCallback(window, CursorPositionCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetScrollCallback(window, MouseScrollCallback);

    int frame_width = 0, frame_height = 0;
    while (!glfwWindowShouldClose(window)) {
        float time = glfwGetTime();

        glfwPollEvents();
        int window_width, window_height;
        glfwGetWindowSize(window, &window_width, &window_height);

        if (window_width == 0 || window_height == 0) {
            continue;
        }

        if (frame_width != window_width || frame_height != window_height) {
            frame_width = window_width;
            frame_height = window_height;
            RefreshSwapchain(glfwGetWin32Window(window), frame_width, frame_height);
        }

        blast::GfxCommandBuffer* cmd = g_device->RequestCommandBuffer(blast::QUEUE_GRAPHICS);

        // update object ub
        object_storages[0].model_matrix = glm::mat4(1.0f);
        object_storages[0].view_matrix = glm::mat4(1.0f);
        object_storages[0].proj_matrix = glm::mat4(1.0f);

        glm::mat4 view_matrix = glm::lookAt(camera.position, camera.position + camera.front, camera.up);

        float fov = glm::radians(60.0);
        float aspect = frame_width / (float)frame_height;
        glm::mat4 proj_matrix = glm::perspective(fov, aspect, 0.1f, 100.0f);
        proj_matrix[1][1] *= -1;

        object_storages[1].model_matrix = glm::mat4(1.0f);
        object_storages[1].view_matrix = view_matrix;
        object_storages[1].proj_matrix = proj_matrix;
        g_device->UpdateBuffer(cmd, object_ub, object_storages.data(), sizeof(ObjectUniforms) * object_storages.size());

        // test pass
        {
            blast::GfxTextureBarrier texture_barriers[2];
            texture_barriers[0].texture = test_texture;
            texture_barriers[0].new_state = blast::RESOURCE_STATE_UNORDERED_ACCESS;
            texture_barriers[1].texture = luminance_texture;
            texture_barriers[1].new_state = blast::RESOURCE_STATE_UNORDERED_ACCESS;
            g_device->SetBarrier(cmd, 0, nullptr, 2, texture_barriers);

            g_device->BindComputeShader(cmd, luminance_shader);

            g_device->BindUAV(cmd, test_texture, 0);

            g_device->BindUAV(cmd, luminance_texture, 1);

            g_device->Dispatch(cmd, std::max(1u, (uint32_t)(test_texture->desc.width) / 16), std::max(1u, (uint32_t)(test_texture->desc.height) / 16), 1);

            texture_barriers[0].texture = test_texture;
            texture_barriers[0].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            texture_barriers[1].texture = luminance_texture;
            texture_barriers[1].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            g_device->SetBarrier(cmd, 0, nullptr, 2, texture_barriers);

        }

        // generate wave
        waves_generator->Update(cmd, time);

        // draw scene
        {
            uint32_t texture_barrier_count = 2;
            blast::GfxTextureBarrier texture_barriers[3];
            texture_barriers[0].texture = scene_color_tex;
            texture_barriers[0].new_state = blast::RESOURCE_STATE_RENDERTARGET;
            texture_barriers[1].texture = scene_depth_tex;
            texture_barriers[1].new_state = blast::RESOURCE_STATE_DEPTH_WRITE;
            if (g_sample_count != blast::SAMPLE_COUNT_1) {
                texture_barriers[2].texture = resolve_tex;
                texture_barriers[2].new_state = blast::RESOURCE_STATE_COPY_DEST;
                texture_barrier_count++;
            }
            g_device->SetBarrier(cmd, 0, nullptr, texture_barrier_count, texture_barriers);

            g_device->RenderPassBegin(cmd, scene_renderpass);

            blast::Viewport viewport;
            viewport.x = 0;
            viewport.y = 0;
            viewport.w = frame_width;
            viewport.h = frame_height;
            g_device->BindViewports(cmd, 1, &viewport);

            blast::Rect rect;
            rect.left = 0;
            rect.top = 0;
            rect.right = frame_width;
            rect.bottom = frame_height;
            g_device->BindScissorRects(cmd, 1, &rect);

            g_device->BindPipeline(cmd, scene_pipeline);

            g_device->BindResource(cmd, waves_generator->GetHeightMap(), 0);

            g_device->BindSampler(cmd, linear_sampler, 0);

            g_device->BindConstantBuffer(cmd, object_ub, 0, sizeof(ObjectUniforms), 0);

            blast::GfxBuffer* vertex_buffers[] = { g_quad_vertex_buffer };
            uint64_t vertex_offsets[] = {0};
            g_device->BindVertexBuffers(cmd, vertex_buffers, 0, 1, vertex_offsets);

            g_device->BindIndexBuffer(cmd, g_quad_index_buffer, blast::IndexType::INDEX_TYPE_UINT32, 0);

            g_device->DrawIndexed(cmd, 6, 0, 0);

            g_device->RenderPassEnd(cmd);

            texture_barrier_count = 2;
            texture_barriers[0].texture = scene_color_tex;
            texture_barriers[0].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            texture_barriers[1].texture = scene_depth_tex;
            texture_barriers[1].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
            if (g_sample_count != blast::SAMPLE_COUNT_1) {
                texture_barriers[2].texture = resolve_tex;
                texture_barriers[2].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
                texture_barrier_count++;
            }
            g_device->SetBarrier(cmd, 0, nullptr, texture_barrier_count, texture_barriers);
        }

        // draw to swapchain
        {
            g_device->RenderPassBegin(cmd, g_swapchain);

            g_device->BindPipeline(cmd, blit_pipeline);

            blast::Viewport viewport;
            viewport.x = 0;
            viewport.y = 0;
            viewport.w = frame_width;
            viewport.h = frame_height;
            g_device->BindViewports(cmd, 1, &viewport);

            blast::Rect rect;
            rect.left = 0;
            rect.top = 0;
            rect.right = frame_width;
            rect.bottom = frame_height;
            g_device->BindScissorRects(cmd, 1, &rect);

            if (g_sample_count != blast::SAMPLE_COUNT_1) {
                g_device->BindResource(cmd, resolve_tex, 0);
            } else {
                g_device->BindResource(cmd, scene_color_tex, 0);
            }

            g_device->BindSampler(cmd, linear_sampler, 0);

            g_device->BindConstantBuffer(cmd, object_ub, 0, sizeof(ObjectUniforms), 0);

            blast::GfxBuffer* vertex_buffers[] = { g_quad_vertex_buffer };
            uint64_t vertex_offsets[] = {0};
            g_device->BindVertexBuffers(cmd, vertex_buffers, 0, 1, vertex_offsets);

            g_device->BindIndexBuffer(cmd, g_quad_index_buffer, blast::IndexType::INDEX_TYPE_UINT32, 0);

            g_device->DrawIndexed(cmd, 6, 0, 0);

            g_device->RenderPassEnd(cmd);
        }

        g_device->SubmitAllCommandBuffer();
    }
    glfwDestroyWindow(window);
    glfwTerminate();

    g_device->DestroyShader(blit_vert_shader);
    g_device->DestroyShader(blit_frag_shader);
    g_device->DestroyShader(scene_vert_shader);
    g_device->DestroyShader(scene_frag_shader);
    g_device->DestroyShader(copy_shader);
    g_device->DestroyShader(fft_shader);
    g_device->DestroyShader(luminance_shader);

    if (scene_renderpass) {
        g_device->DestroyTexture(scene_color_tex);
        g_device->DestroyTexture(scene_depth_tex);
        if (g_sample_count != blast::SAMPLE_COUNT_1) {
            g_device->DestroyTexture(resolve_tex);
        }
        g_device->DestroyRenderPass(scene_renderpass);
    }

    if (blit_pipeline) {
        g_device->DestroyPipeline(blit_pipeline);
    }

    if (scene_pipeline) {
        g_device->DestroyPipeline(scene_pipeline);
    }

    g_device->DestroyBuffer(g_quad_vertex_buffer);
    g_device->DestroyBuffer(g_quad_index_buffer);

    g_device->DestroySampler(linear_sampler);
    g_device->DestroySampler(nearest_sampler);

    g_device->DestroyTexture(test_texture);
    g_device->DestroyTexture(luminance_texture);
    g_device->DestroyTexture(result_texture);

    g_device->DestroyBuffer(object_ub);

    g_device->DestroySwapChain(g_swapchain);

    SAFE_DELETE(waves_generator);

    SAFE_DELETE(g_context);

    SAFE_DELETE(g_device);
    SAFE_DELETE(g_shader_compiler);

    return 0;
}

void RefreshSwapchain(void* window, uint32_t width, uint32_t height) {
    blast::GfxSwapChainDesc swapchain_desc;
    swapchain_desc.window = window;
    swapchain_desc.width = width;
    swapchain_desc.height = height;
    swapchain_desc.clear_color[0] = 1.0f;
    g_swapchain = g_device->CreateSwapChain(swapchain_desc, g_swapchain);

    if (scene_renderpass) {
        g_device->DestroyTexture(scene_color_tex);
        g_device->DestroyTexture(scene_depth_tex);
        if (g_sample_count != blast::SAMPLE_COUNT_1) {
            g_device->DestroyTexture(resolve_tex);
        }
        g_device->DestroyRenderPass(scene_renderpass);
    }
    blast::GfxTextureDesc texture_desc = {};
    texture_desc.width = width;
    texture_desc.height = height;
    texture_desc.sample_count = g_sample_count;
    texture_desc.format = blast::FORMAT_R8G8B8A8_UNORM;
    texture_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
    texture_desc.res_usage = blast::RESOURCE_USAGE_SHADER_RESOURCE | blast::RESOURCE_USAGE_RENDER_TARGET;
    texture_desc.clear.color[0] = 0.0f;
    texture_desc.clear.color[1] = 0.0f;
    texture_desc.clear.color[2] = 0.0f;
    texture_desc.clear.color[3] = 0.0f;
    // 默认深度值为1
    texture_desc.clear.depthstencil.depth = 1.0f;
    scene_color_tex = g_device->CreateTexture(texture_desc);

    if (g_sample_count != blast::SAMPLE_COUNT_1) {
        texture_desc.sample_count = blast::SAMPLE_COUNT_1;
        resolve_tex = g_device->CreateTexture(texture_desc);
    }

    texture_desc.sample_count = g_sample_count;
    texture_desc.format = blast::FORMAT_D24_UNORM_S8_UINT;
    texture_desc.res_usage = blast::RESOURCE_USAGE_SHADER_RESOURCE | blast::RESOURCE_USAGE_DEPTH_STENCIL;
    scene_depth_tex = g_device->CreateTexture(texture_desc);

    blast::GfxRenderPassDesc renderpass_desc = {};
    renderpass_desc.attachments.push_back(blast::RenderPassAttachment::RenderTarget(scene_color_tex, -1, blast::LOAD_CLEAR));
    renderpass_desc.attachments.push_back(
            blast::RenderPassAttachment::DepthStencil(
                    scene_depth_tex,
                    -1,
                    blast::LOAD_CLEAR,
                    blast::STORE_STORE
            )
    );

    if (g_sample_count != blast::SAMPLE_COUNT_1) {
        renderpass_desc.attachments.push_back(blast::RenderPassAttachment::Resolve(resolve_tex));
    }
    scene_renderpass = g_device->CreateRenderPass(renderpass_desc);

    blast::GfxInputLayout input_layout = {};
    blast::GfxInputLayout::Element input_element;
    input_element.semantic = blast::SEMANTIC_POSITION;
    input_element.format = blast::FORMAT_R32G32B32_FLOAT;
    input_element.binding = 0;
    input_element.location = 0;
    input_element.offset = offsetof(Vertex, pos);
    input_layout.elements.push_back(input_element);

    input_element.semantic = blast::SEMANTIC_NORMAL;
    input_element.format = blast::FORMAT_R32G32B32_FLOAT;
    input_element.binding = 0;
    input_element.location = 1;
    input_element.offset = offsetof(Vertex, normal);
    input_layout.elements.push_back(input_element);

    input_element.semantic = blast::SEMANTIC_TEXCOORD0;
    input_element.format = blast::FORMAT_R32G32_FLOAT;
    input_element.binding = 0;
    input_element.location = 2;
    input_element.offset = offsetof(Vertex, uv);
    input_layout.elements.push_back(input_element);

    blast::GfxBlendState blend_state = {};
    blend_state.rt[0].src_factor = blast::BLEND_ONE;
    blend_state.rt[0].dst_factor = blast::BLEND_ZERO;
    blend_state.rt[0].src_factor_alpha = blast::BLEND_ONE;
    blend_state.rt[0].dst_factor_alpha = blast::BLEND_ZERO;

    blast::GfxDepthStencilState depth_stencil_state = {};
    depth_stencil_state.depth_test = true;
    depth_stencil_state.depth_write = true;

    blast::GfxRasterizerState rasterizer_state = {};
    rasterizer_state.cull_mode = blast::CULL_NONE;
    rasterizer_state.front_face = blast::FRONT_FACE_CW;
    rasterizer_state.fill_mode = blast::FILL_SOLID;

    // 创建blit管线
    {
        if (blit_pipeline) {
            g_device->DestroyPipeline(blit_pipeline);
        }

        blast::GfxPipelineDesc pipeline_desc;
        pipeline_desc.sc = g_swapchain;
        pipeline_desc.vs = blit_vert_shader;
        pipeline_desc.fs = blit_frag_shader;
        pipeline_desc.il = &input_layout;
        pipeline_desc.bs = &blend_state;
        pipeline_desc.rs = &rasterizer_state;
        pipeline_desc.dss = &depth_stencil_state;
        pipeline_desc.primitive_topo = blast::PRIMITIVE_TOPO_TRI_LIST;
        blit_pipeline = g_device->CreatePipeline(pipeline_desc);
    }

    // 创建scene管线
    {
        if (scene_pipeline) {
            g_device->DestroyPipeline(scene_pipeline);
        }

        blast::GfxPipelineDesc pipeline_desc;
        pipeline_desc.rp = scene_renderpass;
        pipeline_desc.vs = scene_vert_shader;
        pipeline_desc.fs = scene_frag_shader;
        pipeline_desc.il = &input_layout;
        pipeline_desc.bs = &blend_state;
        pipeline_desc.rs = &rasterizer_state;
        pipeline_desc.dss = &depth_stencil_state;
        pipeline_desc.primitive_topo = blast::PRIMITIVE_TOPO_TRI_LIST;
        pipeline_desc.sample_count = g_sample_count;
        scene_pipeline = g_device->CreatePipeline(pipeline_desc);
    }
}

static std::pair<blast::GfxShader*, blast::GfxShader*> CompileShaderProgram(const std::string& vs_path, const std::string& fs_path) {
    blast::GfxShader* vert_shader = nullptr;
    blast::GfxShader* frag_shader = nullptr;
    {
        blast::ShaderCompileDesc compile_desc;
        compile_desc.code = ReadFileData(vs_path);
        compile_desc.stage = blast::SHADER_STAGE_VERT;
        blast::ShaderCompileResult compile_result = g_shader_compiler->Compile(compile_desc);
        blast::GfxShaderDesc shader_desc;
        shader_desc.stage = blast::SHADER_STAGE_VERT;
        shader_desc.bytecode = compile_result.bytes.data();
        shader_desc.bytecode_length = compile_result.bytes.size() * sizeof(uint32_t);
        vert_shader = g_device->CreateShader(shader_desc);
    }

    {
        blast::ShaderCompileDesc compile_desc;
        compile_desc.code = ReadFileData(fs_path);
        compile_desc.stage = blast::SHADER_STAGE_FRAG;
        blast::ShaderCompileResult compile_result = g_shader_compiler->Compile(compile_desc);
        blast::GfxShaderDesc shader_desc;
        shader_desc.stage = blast::SHADER_STAGE_FRAG;
        shader_desc.bytecode = compile_result.bytes.data();
        shader_desc.bytecode_length = compile_result.bytes.size() * sizeof(uint32_t);
        frag_shader = g_device->CreateShader(shader_desc);
    }

    return std::make_pair(vert_shader, frag_shader);
}

static blast::GfxShader* CompileComputeShader(const std::string& cs_path) {
    blast::GfxShader* comp_shader = nullptr;
    blast::ShaderCompileDesc compile_desc;
    compile_desc.code = ReadFileData(cs_path);
    compile_desc.stage = blast::SHADER_STAGE_COMP;
    blast::ShaderCompileResult compile_result = g_shader_compiler->Compile(compile_desc);
    blast::GfxShaderDesc shader_desc;
    shader_desc.stage = blast::SHADER_STAGE_COMP;
    shader_desc.bytecode = compile_result.bytes.data();
    shader_desc.bytecode_length = compile_result.bytes.size() * sizeof(uint32_t);
    comp_shader = g_device->CreateShader(shader_desc);
    return comp_shader;
}

static void CursorPositionCallback(GLFWwindow* window, double pos_x, double pos_y) {
    if (!camera.grabbing) {
        return;
    }

    glm::vec2 offset = camera.start_point - glm::vec2(pos_x, pos_y);
    camera.start_point = glm::vec2(pos_x, pos_y);

    camera.yaw -= offset.x * 0.1f;
    camera.pitch += offset.y * 0.1f;
    glm::vec3 front;
    front.x = cos(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
    front.y = sin(glm::radians(camera.pitch));
    front.z = sin(glm::radians(camera.yaw)) * cos(glm::radians(camera.pitch));
    camera.front = glm::normalize(front);
    camera.right = glm::normalize(glm::cross(camera.front, glm::vec3(0, 1, 0)));
    camera.up = glm::normalize(glm::cross(camera.right, camera.front));
}

static void MouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
    double x, y;
    glfwGetCursorPos(window, &x, &y);
    switch (action) {
        case 0:
            // Button Up
            if (button == 1) {
                camera.grabbing = false;
            }
            break;
        case 1:
            // Button Down
            if (button == 1) {
                camera.grabbing = true;
                camera.start_point = glm::vec2(x, y);
            }
            break;
        default:
            break;
    }
}

static void MouseScrollCallback(GLFWwindow* window, double offset_x, double offset_y) {
    camera.position += camera.front * (float)offset_y * 0.1f;
}

