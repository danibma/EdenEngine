#include "global.hlsli"
#include "phong_lighting.hlsli"

struct PSInput
{
    float4 position : SV_POSITION;
    float4 positionModel : Position;
    float3 normal : NORMAL;
    float4 color : COLOR;
};

cbuffer SceneData : register(b0)
{
    float4x4 view;
    float4x4 viewProjection;
};

cbuffer Transform : register(b1)
{
    float4x4 transform;
};

// Lights
ConstantBuffer<DirectionalLight> directionalLightCB : register(b2);
ConstantBuffer<PointLight> pointLightCB : register(b3);

//=================
// Vertex Shader
//=================
PSInput VSMain(float3 position : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL, float4 color : COLOR)
{
    PSInput result;

    float4x4 mvpMatrix = mul(viewProjection, transform);
    float4x4 modelViewMatrix = mul(view, transform);
    
    result.position = mul(mvpMatrix, float4(position, 1.0f));
    result.positionModel = mul(modelViewMatrix, float4(position, 1.0f));
    result.normal = TransformDirection(transform, normal);
    result.color = color;

    return result;
}

//=================
// Pixel Shader
//=================
float4 PSMain(PSInput input) : SV_TARGET
{
    float4 objectColor = input.color;

    // Calculate Directional Light
    DirectionalLight dl;
    dl.direction = directionalLightCB.direction;
    float4 pixelColor = CalculateDirectionLight(objectColor, input.positionModel, input.normal, dl);
    
    // Calculate point lights
    PointLight pl;
    pl.position = pointLightCB.position;
    pl.constant_value = pointLightCB.constant_value;
    pl.linear_value = pointLightCB.linear_value;
    pl.quadratic_value = pointLightCB.quadratic_value;
    pixelColor += CalculatePointLight(objectColor, input.positionModel, input.normal, pl);
    
    return pixelColor;
}