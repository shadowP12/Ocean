#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 1000) uniform texture2D result_texture;
layout(binding = 3000) uniform sampler linear_sampler;

layout(location = 0) in vec2 v_uv;

layout(location = 0) out vec4 out_color;

void main()
{
    // magic number to test
    float mn = 100.0;

    float r = texture(sampler2D(result_texture, linear_sampler), v_uv).r;
    out_color = vec4(r / mn);
}