#include "Global.hlsli"
#include "PhongLighting.hlsli"

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
    result.color = color.rgb;
    result.view_dir = normalize(view_position.xyz - result.pixel_pos.xyz);

    return result;
}

//=================
// Pixel Shader
//=================
float4 PSMain(Vertex vertex) : SV_TARGET
{
    float4 diffuse_texture = g_textureDiffuse.Sample(g_linearSampler, vertex.uv);
    float alpha = diffuse_texture.a;
    float4 emissive = g_textureEmissive.Sample(g_linearSampler, vertex.uv);
    
    if (alpha < 0.01f)
        discard;
    
    vertex.color = diffuse_texture.rgb;
    vertex.color += emissive.rgb;
    
    float3 pixel_color = CalculateAllLights(vertex, DirectionalLights, PointLights);
    
    return float4(pixel_color, alpha);
}