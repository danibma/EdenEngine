#pragma once

struct Vertex
{
    float4 position : SV_POSITION;
    float4 pixel_pos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float4 color : COLOR;
    float3 view_dir : VIEW_DIR;
};

cbuffer SceneData
{
    float4x4 view;
    float4x4 view_projection;
    float4 view_position;
};

cbuffer Transform
{
    float4x4 transform;
};

// https://github.com/graphitemaster/normals_revisited
float3 TransformDirection(in float4x4 transform, in float3 direction)
{
    float4x4 result;
#define minor(m, r0, r1, r2, c0, c1, c2)                            \
        (m[c0][r0] * (m[c1][r1] * m[c2][r2] - m[c1][r2] * m[c2][r1]) -  \
         m[c1][r0] * (m[c0][r1] * m[c2][r2] - m[c0][r2] * m[c2][r1]) +  \
         m[c2][r0] * (m[c0][r1] * m[c1][r2] - m[c0][r2] * m[c1][r1]))
    result[0][0] = minor(transform, 1, 2, 3, 1, 2, 3);
    result[1][0] = -minor(transform, 1, 2, 3, 0, 2, 3);
    result[2][0] = minor(transform, 1, 2, 3, 0, 1, 3);
    result[3][0] = -minor(transform, 1, 2, 3, 0, 1, 2);
    result[0][1] = -minor(transform, 0, 2, 3, 1, 2, 3);
    result[1][1] = minor(transform, 0, 2, 3, 0, 2, 3);
    result[2][1] = -minor(transform, 0, 2, 3, 0, 1, 3);
    result[3][1] = minor(transform, 0, 2, 3, 0, 1, 2);
    result[0][2] = minor(transform, 0, 1, 3, 1, 2, 3);
    result[1][2] = -minor(transform, 0, 1, 3, 0, 2, 3);
    result[2][2] = minor(transform, 0, 1, 3, 0, 1, 3);
    result[3][2] = -minor(transform, 0, 1, 3, 0, 1, 2);
    result[0][3] = -minor(transform, 0, 1, 2, 1, 2, 3);
    result[1][3] = minor(transform, 0, 1, 2, 0, 2, 3);
    result[2][3] = -minor(transform, 0, 1, 2, 0, 1, 3);
    result[3][3] = minor(transform, 0, 1, 2, 0, 1, 2);
    return mul(result, float4(direction, 0.0f)).xyz;
#undef minor    // cleanup
}