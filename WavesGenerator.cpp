#include "WavesGenerator.h"

WavesGenerator::WavesGenerator(Context* in_context, int in_size, int in_length) {
    context = in_context;
    size = in_size;
    length = in_length;

    // init params
    wave_amp = 0.0003f;
    wind_speed = glm::vec2(32.0f, 32.0f);

    dispersion_table = new float[size * size];
    spectrum = new glm::vec2[size * size];
    spectrum_conj = new glm::vec2[size * size];
    height_data = new glm::vec2[size * size];

    for (int n = 0; n < size; n++) {
        for (int m = 0; m < size; m++) {
            int index = n * size + m;

            dispersion_table[index] = Dispersion(n, m);

            spectrum[index] = InitSpectrum(n, m);

            spectrum_conj[index] = InitSpectrum(-n, -m);
            spectrum_conj[index].y *= -1.0f;
        }
    }

    fft = new FourierTransform(context, size);

    blast::GfxTextureDesc texture_desc;
    texture_desc.width = size;
    texture_desc.height = size;
    texture_desc.format = blast::FORMAT_R32G32_FLOAT;
    texture_desc.mem_usage = blast::MEMORY_USAGE_GPU_ONLY;
    texture_desc.res_usage = blast::RESOURCE_USAGE_SHADER_RESOURCE | blast::RESOURCE_USAGE_UNORDERED_ACCESS;
    height_map = context->device->CreateTexture(texture_desc);
}

WavesGenerator::~WavesGenerator() {
    SAFE_DELETE_ARRAY(dispersion_table);
    SAFE_DELETE_ARRAY(height_data);
    SAFE_DELETE_ARRAY(spectrum);
    SAFE_DELETE_ARRAY(spectrum_conj);
    SAFE_DELETE(fft);
    context->device->DestroyTexture(height_map);
}

glm::vec2 WavesGenerator::RandomVariable() {
    float x1, x2, w;
    do {
        x1 = 2.f * UniformRandomVariable() - 1.f;
        x2 = 2.f * UniformRandomVariable() - 1.f;
        w = x1 * x1 + x2 * x2;
    } while ( w >= 1.f );
    w = sqrt((-2.f * log(w)) / w);
    return glm::vec2(x1 * w, x2 * w);
}

float WavesGenerator::Dispersion(int n, int m) {
    float kx = PI * (2.0f * n - size) / length;
    float kz = PI * (2.0f * m - size) / length;
    return glm::floor(glm::sqrt(9.8f * glm::sqrt(kx * kx + kz * kz)));
}

float WavesGenerator::PhillipsSpectrum(int n, int m) {
    glm::vec2 k = glm::vec2(PI * (2 * n - size) / length, PI * (2 * m - size) / length);
    float k_length = glm::length(k);

    if (k_length < 0.000001f) return 0.0f;

    float k_length2 = k_length  * k_length;
    float k_length4 = k_length2 * k_length2;
    float k_dot_w   = glm::dot(glm::normalize(k), glm::normalize(wind_speed));
    float k_dot_w2  = k_dot_w * k_dot_w * k_dot_w * k_dot_w * k_dot_w * k_dot_w;

    float w_length = glm::length(wind_speed);
    float L = w_length * w_length / 9.8f;
    float L2 = L * L;

    float damping = 0.001f;
    float l2 = L2 * damping * damping;

    return wave_amp * glm::exp(-1.0f / (k_length2 * L2)) / k_length4 * k_dot_w2 * glm::exp(-k_length2 * l2);
}

glm::vec2 WavesGenerator::InitSpectrum(int n, int m) {
    glm::vec2 r = RandomVariable();
    return r * glm::sqrt(PhillipsSpectrum(n, m) / 2.0f);
}

glm::vec2 WavesGenerator::UpdateSpectrum(float t, int n, int m) {
    int index = n * size + m;

    float omegat = dispersion_table[index] * t;

    float cos = glm::cos(omegat);
    float sin = glm::sin(omegat);

    float c0a = spectrum[index].x*cos - spectrum[index].y*sin;
    float c0b = spectrum[index].x*sin + spectrum[index].y*cos;

    float c1a = spectrum_conj[index].x*cos - spectrum_conj[index].y*-sin;
    float c1b = spectrum_conj[index].x*-sin + spectrum_conj[index].y*cos;

    return glm::vec2(c0a+c1a, c0b+c1b);
}

void WavesGenerator::Update(blast::GfxCommandBuffer* cmd , float t) {
    float kx, kz, len, lambda = -1.0f;
    int index, index1;

    for (int n = 0; n < size; n++) {
        kx = PI * (2.0f * n - size) / length;

        for (int m = 0; m < size; m++) {
            kz = PI*(2 * m - size) / length;
            len = glm::sqrt(kx * kx + kz * kz);
            index = n * size + m;

            height_data[index] = UpdateSpectrum(t, n, m);
        }
    }

    blast::GfxTextureBarrier barrier;
    barrier.texture = height_map;
    barrier.new_state = blast::RESOURCE_STATE_COPY_DEST;
    context->device->SetBarrier(cmd, 0, nullptr, 1, &barrier);

    context->device->UpdateTexture(cmd, height_map, height_data);

    barrier.texture = height_map;
    barrier.new_state = blast::RESOURCE_STATE_SHADER_RESOURCE;
    context->device->SetBarrier(cmd, 0, nullptr, 1, &barrier);

    fft->Execute(cmd, height_map, height_map);
}