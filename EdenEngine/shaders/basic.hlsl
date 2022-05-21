#include "global.hlsli"

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
    float4 lightPosition;
};

cbuffer Transform : register(b1)
{
    float4x4 transform;
};

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
    // Lighting
    float3 lightColor = float3(1.0f, 1.0f, 1.0f);
    float3 lightPos = lightPosition.xyz;
    float3 fragPos = float3(input.positionModel.xyz);
    
    // Ambient Light
    float ambientStrength = 0.1f;
    float4 ambient = float4(ambientStrength * lightColor, 1.0f) * input.color;
    
    // Diffuse Light
    float3 norm = normalize(input.normal);
    float3 lightDirection = normalize(lightPos - fragPos);
    float diffuseColor = max(dot(norm, lightDirection), 0.0f);
    float4 diffuse = float4(diffuseColor * lightColor, 1.0f) * input.color;
    
    // Specular Light
    float specularStrength = 0.5f;
    float3 viewDirection = normalize(-fragPos);
    float3 reflectDirection = reflect(-lightDirection, norm);
    float shininess = 32;
    float spec = pow(max(dot(viewDirection, reflectDirection), 0.0f), shininess);
    float4 specular = float4((specularStrength * spec * lightColor), 1.0f);
    
    float4 pixelColor = (ambient + diffuse + specular);
    
    return pixelColor;
}