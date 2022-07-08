#include "Global.hlsli"

Texture2D g_sceneTexture : register(t0);
SamplerState g_linearSampler : register(s0);

cbuffer SceneSettings
{
    float exposure;
};

float3 GammaCorrect(float3 color, float gamma)
{
    return pow(color, (float3)1.0f/gamma);
}

// Based on http://www.oscars.org/science-technology/sci-tech-projects/aces
// and taken from Hazel Engine SceneComposite.glsl shader
float3 ACESTonemap(float3 color)
{
    float3x3 m1 = float3x3(
		0.59719, 0.07600, 0.02840,
		0.35458, 0.90834, 0.13383,
		0.04823, 0.01566, 0.83777
	);
    float3x3 m2 = float3x3(
		1.60475, -0.10208, -0.00327,
		-0.53108, 1.10813, -0.07276,
		-0.07367, -0.00605, 1.07602
	);
    float3 v = mul(color, m1);
    float3 a = v * (v + 0.0245786) - 0.000090537;
    float3 b = v * (0.983729 * v + 0.4329510) + 0.238081;
    return clamp(mul((a / b), m2), 0.0, 1.0);
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
    
    float3 final_color = g_sceneTexture.Sample(g_linearSampler, vertex.uv).rgb;
    
    final_color *= exposure;
    final_color = ACESTonemap(final_color.rgb);
    final_color = GammaCorrect(final_color, gamma);
    return float4(final_color, 1.0f);
}