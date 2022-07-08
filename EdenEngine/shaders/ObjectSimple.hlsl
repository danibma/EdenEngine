#include "Global.hlsli"
#include "PhongLighting.hlsli"

// Lights
StructuredBuffer<DirectionalLight> DirectionalLights;
StructuredBuffer<PointLight> PointLights;

//=================
// Vertex Shader
//=================
Vertex VSMain(float3 position : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL, float4 color : COLOR)
{
    Vertex result;

    float4x4 mvpMatrix = mul(view_projection, transform);
    
    result.position = mul(mvpMatrix, float4(position, 1.0f));
    result.pixel_pos = mul(transform, float4(position, 1.0f));
    result.normal = TransformDirection(transform, normal);
    result.color = color;
    result.view_dir = normalize(view_position.xyz - result.pixel_pos.xyz);
    
    return result;
}

//=================
// Pixel Shader
//=================
float4 PSMain(Vertex input) : SV_TARGET
{
    float4 object_color = input.color;
    float3 view_dir = normalize(view_position.xyz - input.pixel_pos.xyz);

    float4 pixel_color = CalculateAllLights(input, DirectionalLights, PointLights);
    
    return pixel_color;
}