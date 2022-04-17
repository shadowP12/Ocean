#pragma once

#include "OceanDefine.h"
#include "FourierTransform.h"

#include <Blast/Gfx/GfxDefine.h>
#include <Blast/Gfx/GfxDevice.h>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

class WavesGenerator {
public:
    WavesGenerator(Context* context, int size, int length);

    ~WavesGenerator();

    void Update(blast::GfxCommandBuffer* cmd, float t);

    blast::GfxTexture* GetHeightMap() { return height_map; }

private:
    glm::vec2 RandomVariable();

    float Dispersion(int n, int m);

    float PhillipsSpectrum(int n, int m);

    glm::vec2 InitSpectrum(int n, int m);

    glm::vec2 UpdateSpectrum(float t, int n, int m);

private:
    int size = 0;
    int length = 0;
    float wave_amp = 0.0f;
    float* dispersion_table = nullptr;
    glm::vec2 wind_speed = glm::vec2(0.0f);
    glm::vec2* spectrum = nullptr;
    glm::vec2* spectrum_conj = nullptr;
    glm::vec2* height_data = nullptr;
    blast::GfxTexture* height_map = nullptr;
    Context* context = nullptr;
    FourierTransform* fft = nullptr;
};