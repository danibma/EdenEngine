#pragma once

#define ENABLE_PBR

#define PI          3.14159265358979323846
#define TWO_PI      6.28318530717958647693

// Samplers
SamplerState LinearClamp : register(s0);
SamplerState LinearWrap : register(s1);

struct Vertex
{
    float4 position : SV_POSITION;
    float4 pixelPos : POSITION;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
    float3 color : COLOR;
    float3 viewDir : VIEW_DIR;
};

cbuffer Material
{
	Texture2D g_AlbedoMap 	         : register(t0);
	Texture2D g_NormalMap 	         : register(t1);
	Texture2D g_AOMap 		         : register(t2);
	Texture2D g_EmissiveMap          : register(t3);
    Texture2D g_MetallicRoughnessMap : register(t4);
};

cbuffer SceneData
{
    float4x4 view;
    float4x4 viewProjection;
    float4 viewPosition;
};

cbuffer Transform
{
    float4x4 transform;
};

cbuffer RenderingInfo
{
    float2 g_Resolution : packoffset(c0);
    float g_Time;
}

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