#include "OceanDefine.h"

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

blast::SampleCount g_sample_count = blast::SAMPLE_COUNT_4;

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

int main() {
    g_shader_compiler = new blast::VulkanShaderCompiler();

    g_device = new blast::VulkanDevice();

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(800, 600, "Lightmapper", nullptr, nullptr);
    glfwSetCursorPosCallback(window, CursorPositionCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetScrollCallback(window, MouseScrollCallback);

    int frame_width = 0, frame_height = 0;
    while (!glfwWindowShouldClose(window)) {
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

        g_device->RenderPassBegin(cmd, g_swapchain);
        g_device->RenderPassEnd(cmd);

        g_device->SubmitAllCommandBuffer();
    }
    glfwDestroyWindow(window);
    glfwTerminate();

    g_device->DestroySwapChain(g_swapchain);

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