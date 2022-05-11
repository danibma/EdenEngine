struct PSInput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD;
    float4 normal : NORMAL;
};

cbuffer SceneData : register(b0)
{
    float4x4 mvpMatrix;
};

Texture2D g_textureDiffuse : register(t0);
SamplerState g_samplerDiffuse : register(s0);

// NOTE(Daniel): Loading normal maps correctly, just not using them because we have no light
Texture2D g_textureNormal : register(t1);
SamplerState g_samplerNormal : register(s1);

PSInput VSMain(float3 position : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL)
{
    PSInput result;

    result.position = mul(mvpMatrix, float4(position, 1.0f));
    result.normal = float4(normal, 1.0f);
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    return g_textureDiffuse.Sample(g_samplerDiffuse, input.uv);
}