#include "FourierTransform.h"
#include "OceanDefine.h"

#define USE_FFT_V2 0

struct FFTParam {
    int size;
    int pass;
    int ping_pong;
    int is_horizontal;
};

int BitReverse(int i, int size) {
    int j = i;
    int sum = 0;
    int w = 1;
    int m = size / 2;
    while (m != 0)
    {
        j = ((i & m) > m - 1) ? 1 : 0;
        sum += j * w;
        w *= 2;
        m /= 2;
    }
    return sum;
}

FourierTransform::FourierTransform(Context* in_context, int in_size) {
    size = in_size;
    context = in_context;
    passes = (int)(log(size) / log(2));

    blast::GfxDevice* device = context->device;
    blast::GfxTextureDesc texture_desc;
    texture_desc.width = size;
    texture_desc.height = size;
    texture_desc.format = blast::FORMAT_R32G32_FLOAT;
    texture_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
    texture_desc.res_usage = blast::RESOURCE_USAGE_SHADER_RESOURCE | blast::RESOURCE_USAGE_UNORDERED_ACCESS;
    pass_texture0 = device->CreateTexture(texture_desc);
    pass_texture1 = device->CreateTexture(texture_desc);

    blast::GfxBufferDesc buffer_desc = {};
    buffer_desc.size = sizeof(LookUp) * size * passes;
    buffer_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
    buffer_desc.res_usage = blast::RESOURCE_USAGE_RW_BUFFER;
    butterfly_lookup_table = device->CreateBuffer(buffer_desc);

    LookUp* butterfly_lookup_table_data = new LookUp[size * passes];
    for (int i = 0; i < passes; i++) {
        int blocks = (int)pow(2, passes - 1 - i);
        int inputs = (int)pow(2, i);

        for (int j = 0; j < blocks; j++){
            for (int k = 0; k < inputs; k++) {
                int i1, i2, j1, j2;
                if (i == 0) {
                    i1 = j * inputs * 2 + k;
                    i2 = j * inputs * 2 + inputs + k;
                    j1 = BitReverse(i1, size);
                    j2 = BitReverse(i2, size);
                } else {
                    i1 = j * inputs * 2 + k;
                    i2 = j * inputs * 2 + inputs + k;
                    j1 = i1;
                    j2 = i2;
                }

                float wr = cos(2.0f * PI * (float)(k * blocks) / size);
                float wi = -sin(2.0f * PI * (float)(k * blocks) / size);

                int offset1 = (i1 + i * size);
                butterfly_lookup_table_data[offset1].j1 = j1;
                butterfly_lookup_table_data[offset1].j2 = j2;
                butterfly_lookup_table_data[offset1].wr = wr;
                butterfly_lookup_table_data[offset1].wi = wi;

                int offset2 = (i2 + i * size);
                butterfly_lookup_table_data[offset2].j1 = j1;
                butterfly_lookup_table_data[offset2].j2 = j2;
                butterfly_lookup_table_data[offset2].wr = -wr;
                butterfly_lookup_table_data[offset2].wi = -wi;
            }
        }
    }

    blast::GfxCommandBuffer* copy_cmd = device->RequestCommandBuffer(blast::QUEUE_COPY);
    device->UpdateBuffer(copy_cmd, butterfly_lookup_table, butterfly_lookup_table_data, sizeof(LookUp) * size * passes);
    blast::GfxBufferBarrier barrier;
    barrier.buffer = butterfly_lookup_table;
    barrier.new_state = blast::RESOURCE_STATE_SHADER_RESOURCE | blast::RESOURCE_STATE_UNORDERED_ACCESS;
    device->SetBarrier(copy_cmd, 1, &barrier, 0, nullptr);

    SAFE_DELETE_ARRAY(butterfly_lookup_table_data);
}

FourierTransform::~FourierTransform() {
    blast::GfxDevice* device = context->device;
    device->DestroyTexture(pass_texture0);
    device->DestroyTexture(pass_texture1);
    device->DestroyBuffer(butterfly_lookup_table);
}

void FourierTransform::Execute(blast::GfxCommandBuffer* cmd, blast::GfxTexture* in, blast::GfxTexture* out) {
    blast::GfxDevice* device = context->device;

    blast::GfxTextureBarrier texture_barriers[4];
    texture_barriers[0].texture = in;
    texture_barriers[0].new_state = blast::RESOURCE_STATE_UNORDERED_ACCESS;
    texture_barriers[1].texture = out;
    texture_barriers[1].new_state = blast::RESOURCE_STATE_UNORDERED_ACCESS;
    texture_barriers[2].texture = pass_texture0;
    texture_barriers[2].new_state = blast::RESOURCE_STATE_UNORDERED_ACCESS;
    texture_barriers[3].texture = pass_texture1;
    texture_barriers[3].new_state = blast::RESOURCE_STATE_UNORDERED_ACCESS;
    device->SetBarrier(cmd, 0, nullptr, 4, texture_barriers);

    // Copy To In
    device->BindComputeShader(cmd, context->copy_shader);

    device->BindUAV(cmd, in, 0);

    device->BindUAV(cmd, pass_texture0, 1);

    device->Dispatch(cmd, std::max(1u, (uint32_t)(size) / 16), std::max(1u, (uint32_t)(size) / 16), 1);

    FFTParam fft_param;
    fft_param.size = size;
    fft_param.pass = 0;
    fft_param.ping_pong = false;
    fft_param.is_horizontal = true;

    //Horizontal Step
    for (int i = 0; i < passes; ++i) {
        fft_param.pass = i;
        fft_param.ping_pong = !fft_param.ping_pong;

        device->BindComputeShader(cmd, context->fft_shader);

        device->BindUAV(cmd, pass_texture0, 0);

        device->BindUAV(cmd, pass_texture1, 1);

        device->BindUAV(cmd, butterfly_lookup_table, 2);

        device->PushConstants(cmd, &fft_param, sizeof(FFTParam));

        device->Dispatch(cmd, std::max(1u, (uint32_t)(size) / 16), std::max(1u, (uint32_t)(size) / 16), 1);
    }

    //Vertical Step
    fft_param.is_horizontal = false;
    for (int i = 0; i < passes; ++i) {
        fft_param.pass = i;
        fft_param.ping_pong = !fft_param.ping_pong;

        device->BindComputeShader(cmd, context->fft_shader);

        device->BindUAV(cmd, pass_texture0, 0);

        device->BindUAV(cmd, pass_texture1, 1);

        device->BindUAV(cmd, butterfly_lookup_table, 2);

        device->PushConstants(cmd, &fft_param, sizeof(FFTParam));

        device->Dispatch(cmd, std::max(1u, (uint32_t)(size) / 16), std::max(1u, (uint32_t)(size) / 16), 1);
    }

    // Copy To Out
    device->BindComputeShader(cmd, context->copy_shader);

    if (fft_param.ping_pong) {
        device->BindUAV(cmd, pass_texture1, 0);
    } else {
        device->BindUAV(cmd, pass_texture0, 0);
    }

    device->BindUAV(cmd, out, 1);

    device->Dispatch(cmd, std::max(1u, (uint32_t)(size) / 16), std::max(1u, (uint32_t)(size) / 16), 1);

    texture_barriers[0].texture = in;
    texture_barriers[0].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
    texture_barriers[1].texture = out;
    texture_barriers[1].new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
    device->SetBarrier(cmd, 0, nullptr, 2, texture_barriers);
}