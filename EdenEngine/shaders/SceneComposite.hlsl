#include "Global.hlsli"

Texture2D g_sceneTexture : register(t0);
SamplerState g_linearSampler : register(s0);

float4 GammaCorrect(float4 color, float gamma)
{
    return pow(color, (float4) 1.0f / gamma);
}

//=================
// Vertex Shader
//=================
Vertex VSMain(float2 position : POSITION, float2 uv : TEXCOORD)
{
    Vertex result;
    result.position = float4(position.x, position.y, 0.0f, 1.0f);
    result.uv = -uv;
    return result;
}

//=================
// Pixel Shader
//=================
float4 PSMain(Vertex vertex) : SV_TARGET
{
    const float gamma = 2.2f;
    
    float4 final_color = g_sceneTexture.Sample(g_linearSampler, vertex.uv);
    final_color = GammaCorrect(final_color, gamma);
    return final_color;
}