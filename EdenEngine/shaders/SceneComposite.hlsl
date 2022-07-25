#include "Global.hlsli"

Texture2D g_sceneTexture : register(t0);

cbuffer SceneSettings
{
    float exposure;
    float padding; // This is here only because I don't want this to be considered a Constant by the root signature
};

float3 GammaCorrect(float3 color, float gamma)
{
    return pow(color, (float3)1.0f/gamma);
}

// https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
float3 AcesFilm(const float3 x)
{
    const float a = 2.51;
    const float b = 0.03;
    const float c = 2.43;
    const float d = 0.59;
    const float e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
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
    
    float3 final_color = g_sceneTexture.Sample(LinearWrap, vertex.uv).rgb;
    
    final_color *= exposure;
    final_color = AcesFilm(final_color);
    final_color = GammaCorrect(final_color, gamma);
    return float4(final_color, 1.0f);
}