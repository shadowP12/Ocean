#pragma once

#include <Blast/Gfx/GfxDefine.h>
#include <Blast/Gfx/GfxDevice.h>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm.hpp>
#include <gtx/quaternion.hpp>
#include <gtc/matrix_transform.hpp>

#include <am_fft.h>

static float gInfinity = std::numeric_limits<float>::infinity();
static float gNegInfinity = -gInfinity;
static float gEpsilon = std::numeric_limits<float>::epsilon();

#define PI 3.1415926

#define SAFE_DELETE(x) \
    { \
        delete x; \
        x = nullptr; \
    }

#define SAFE_DELETE_ARRAY(x) \
    { \
        delete[] x; \
        x = nullptr; \
    }

#define MAX_GRID_SIZE 128

inline uint32_t murmur3(const uint32_t* key, size_t wordCount, uint32_t seed) noexcept {
    uint32_t h = seed;
    size_t i = wordCount;
    do {
        uint32_t k = *key++;
        k *= 0xcc9e2d51u;
        k = (k << 15u) | (k >> 17u);
        k *= 0x1b873593u;
        h ^= k;
        h = (h << 13u) | (h >> 19u);
        h = (h * 5u) + 0xe6546b64u;
    } while (--i);
    h ^= wordCount;
    h ^= h >> 16u;
    h *= 0x85ebca6bu;
    h ^= h >> 13u;
    h *= 0xc2b2ae35u;
    h ^= h >> 16u;
    return h;
}

template<typename T>
struct MurmurHash {
    uint32_t operator()(const T& key) const noexcept {
        static_assert(0 == (sizeof(key) & 3u), "Hashing requires a size that is a multiple of 4.");
        return murmur3((const uint32_t*) &key, sizeof(key) / 4, 0);
    }
};

struct Vertex {
    float pos[3];
    float normal[3];
    float uv[2];
};

struct Context {
    blast::GfxDevice* device;
    blast::GfxShader* copy_shader;
    blast::GfxShader* fft_shader;
};

#define RAND_MAX 0x7fff

inline float UniformRandomVariable() {
    return (float)rand()/RAND_MAX;
}