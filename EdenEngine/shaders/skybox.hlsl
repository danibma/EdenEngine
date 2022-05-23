struct PSInput
{
    float4 position : SV_POSITION;
    float3 uv : TEXCOORD;
};

cbuffer SkyboxData : register(b0)
{
    float4x4 viewProjection;
};

//=================
// Vertex Shader
//=================
PSInput VSMain(float3 position : POSITION)
{
    PSInput result;
    result.position = mul(viewProjection, float4(position, 1.0f));
    result.uv = position.xyz;
    
    return result;
}

Texture2D g_cubemapTexture: register(t0);
SamplerState g_linearSampler : register(s0);


float2 SampleSphericalMap(float3 v)
{
    const float2 invAtan = float2(0.1591, 0.3183);
    float2 uv = float2(atan2(v.z, v.x), asin(-v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

//=================
// Pixel Shader
//=================
float4 PSMain(PSInput input) : SV_Target
{
    float2 uv = SampleSphericalMap(normalize(input.uv));
    float4 color = g_cubemapTexture.Sample(g_linearSampler, uv);

    return color;
}