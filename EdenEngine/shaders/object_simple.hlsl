#include "global.hlsli"
#include "phong_lighting.hlsli"

struct PSInput
{
    float4 position : SV_POSITION;
    float4 frag_pos : Position;
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

// Lights
ConstantBuffer<DirectionalLight> DirectionalLightCB : register(b2);
StructuredBuffer<PointLight> PointLights;

//=================
// Vertex Shader
//=================
PSInput VSMain(float3 position : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL, float4 color : COLOR)
{
    PSInput result;

    float4x4 mvpMatrix = mul(view_projection, transform);
    
    result.position = mul(mvpMatrix, float4(position, 1.0f));
    result.frag_pos = mul(transform, float4(position, 1.0f));
    result.normal = TransformDirection(transform, normal);
    result.color = color;

    return result;
}

//=================
// Pixel Shader
//=================
float4 PSMain(PSInput input) : SV_TARGET
{
    float4 object_color = input.color;
    float3 view_dir = normalize(view_position.xyz - input.frag_pos.xyz);

    // Calculate Directional Light
    DirectionalLight dl;
    dl.direction = DirectionalLightCB.direction;
    float4 pixel_color = CalculateDirectionLight(object_color, input.frag_pos, view_dir, input.normal, dl);
    
    // Calculate point lights
    uint num_structs, stride;
    PointLights.GetDimensions(num_structs, stride);
    for (int i = 0; i < num_structs; ++i)
    {
        PointLight pl;
        pl.color = PointLights[i].color;
        pl.position = PointLights[i].position;
        pl.constant_value = PointLights[i].constant_value;
        pl.linear_value = PointLights[i].linear_value;
        pl.quadratic_value = PointLights[i].quadratic_value;
        pixel_color += CalculatePointLight(object_color, input.frag_pos, view_dir, input.normal, pl);
    }
    
    return pixel_color;
}