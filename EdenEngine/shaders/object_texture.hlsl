#include "global.hlsli"
#include "phong_lighting.hlsli"

struct PSInput
{
    float4 position : SV_POSITION;
    float4 frag_pos : Position;
    float2 uv       : TEXCOORD;
    float3 normal   : NORMAL;
    float4 color    : COLOR;
};

cbuffer SceneData : register(b0)
{
    float4x4 view;
    float4x4 view_projection;
    float4   view_position;
};

cbuffer Transform : register(b1)
{
    float4x4 transform;
};

Texture2D g_textureDiffuse : register(t0);
Texture2D g_textureEmissive : register(t1);
SamplerState g_linearSampler : register(s0);

// Lights
ConstantBuffer<DirectionalLight> DirectionalLightCB : register(b2);
StructuredBuffer<PointLight> PointLights;

//=================
// Vertex Shader
//=================
PSInput VSMain(float3 position : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL, float4 color : COLOR)
{
    PSInput result;

    float4x4 mvp_matrix = mul(view_projection, transform);
    
    result.position = mul(mvp_matrix, float4(position, 1.0f));
    result.frag_pos = mul(transform, float4(position, 1.0f));
    result.normal = TransformDirection(transform, normal);
    result.uv = uv;
    result.color = color;

    return result;
}

//=================
// Pixel Shader
//=================
float4 PSMain(PSInput input) : SV_TARGET
{
    float4 diffuse_texture = g_textureDiffuse.Sample(g_linearSampler, input.uv);
    float4 object_color = diffuse_texture;
    float3 view_dir = normalize(view_position.xyz - input.frag_pos.xyz);
    
    if (diffuse_texture.x == 0.0f &&
        diffuse_texture.y == 0.0f &&
        diffuse_texture.z == 0.0f)
        object_color = input.color;
    
    // Emissive
    float4 emissive = g_textureEmissive.Sample(g_linearSampler, input.uv).rgba * 3.0f;
    
    // Calculate Directional Light
    float4 pixel_color = CalculateDirectionLight(object_color, input.frag_pos, view_dir, input.normal, DirectionalLightCB);

    // Calculate point lights
    uint num_structs, stride;
    PointLights.GetDimensions(num_structs, stride);
    for (int i = 0; i < num_structs; ++i)
        pixel_color += CalculatePointLight(object_color, input.frag_pos, view_dir, input.normal, PointLights[i]);
    
    pixel_color += emissive;
    
    
    return pixel_color;
}