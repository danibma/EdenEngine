#include "global.hlsli"
#include "phong_lighting.hlsli"

Texture2D g_textureDiffuse : register(t0);
Texture2D g_textureEmissive : register(t1);
SamplerState g_linearSampler : register(s0);

// Lights
StructuredBuffer<DirectionalLight> DirectionalLights;
StructuredBuffer<PointLight> PointLights;

//=================
// Vertex Shader
//=================
Vertex VSMain(float3 position : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL, float4 color : COLOR)
{
    Vertex result;

    float4x4 mvp_matrix = mul(view_projection, transform);
    
    result.position = mul(mvp_matrix, float4(position, 1.0f));
    result.pixel_pos = mul(transform, float4(position, 1.0f));
    result.normal = TransformDirection(transform, normal);
    result.uv = uv;
    result.color = color;
    result.view_dir = normalize(view_position.xyz - result.pixel_pos.xyz);

    return result;
}

//=================
// Pixel Shader
//=================
float4 PSMain(Vertex vertex) : SV_TARGET
{
    // TODO: Check if uv values are zero when it loads the mesh, so we can create only one pipeline
    float4 diffuse_texture = g_textureDiffuse.Sample(g_linearSampler, vertex.uv);
    
    if (diffuse_texture.a > 0.0f)
        vertex.color = diffuse_texture;
    
    // Emissive
    float4 emissive = g_textureEmissive.Sample(g_linearSampler, vertex.uv).rgba * 3.0f;
    
    float4 pixel_color = CalculateAllLights(vertex, DirectionalLights, PointLights);
    pixel_color += emissive;
    
    return pixel_color;
}