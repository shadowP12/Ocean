#pragma once

#include <Blast/Gfx/GfxDefine.h>
#include <Blast/Gfx/GfxDevice.h>

#include <glm.hpp>
#include <gtc/matrix_transform.hpp>

class FourierTransform {
public:
    FourierTransform(blast::GfxDevice* device, int size);

    ~FourierTransform();

    void Execute(blast::GfxCommandBuffer* cmd, blast::GfxTexture* in, blast::GfxTexture* out);

private:
    blast::GfxDevice* device = nullptr;
    blast::GfxBuffer* butterfly_lookup_table = nullptr;
};