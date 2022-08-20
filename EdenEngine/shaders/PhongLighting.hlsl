#include "Global.hlsli"

// blinn-phong define
#define BLINN 1

//=================
// Directional Light
//=================
struct DirectionalLight
{
    float4 direction;
    float intensity;
};
StructuredBuffer<DirectionalLight> DirectionalLights;

float3 CalculateDirectionLight(Vertex vertex, DirectionalLight directional_light)
{
    // Lighting
    float3 lightColor = float3(1.0f, 1.0f, 1.0f);
    
    // Ambient Light
    float ambientStrength = 0.1f;
    float3 ambient = ambientStrength * lightColor * vertex.color;
    
    // Diffuse Light
    float3 norm = normalize(vertex.normal);
    float3 lightDir = normalize(directional_light.direction.xyz);
    float diffuseColor = max(dot(norm, lightDir), 0.0f);
    float3 diffuse = diffuseColor * lightColor * vertex.color;
    
    // Specular Light
    float specularStrength = 0.1f;
    float shininess = 32.0f;
#if BLINN
    float3 halfwayDir = normalize(lightDir + vertex.viewDir);
    float spec = pow(max(dot(vertex.viewDir, halfwayDir), 0.0f), shininess);
#else
    float3 reflectDirection = reflect(-lightDir, norm);
    float spec = pow(max(dot(vertex.viewDir, reflectDirection), 0.0f), shininess);
#endif
    float3 specular = (specularStrength * spec * lightColor);

    return (ambient + diffuse + specular) * directional_light.intensity;
}

//=================
// Point Light
//=================
struct PointLight
{
    float4 color;
    float4 position;
    float intensity;
};
StructuredBuffer<PointLight> PointLights;

float3 CalculatePointLight(Vertex vertex, PointLight pointLight)
{
    // Lighting
    float3 lightColor = pointLight.color.rgb;
    
    // Ambient Light
    float ambientStrength = 0.1f;
    float3 ambient = ambientStrength * lightColor * vertex.color;
    
    // Diffuse Light
    float3 norm = normalize(vertex.normal);
    float3 lightDir = normalize(pointLight.position.xyz - vertex.pixelPos.xyz);
    float diffuseColor = max(dot(norm, lightDir), 0.0f);
    float3 diffuse = diffuseColor * lightColor * vertex.color;
    
    // Specular Light
    float specularStrength = 0.1f;
    float shininess = 32.0f;
#if BLINN
    float3 halfwayDir = normalize(lightDir + vertex.viewDir);
    float spec = pow(max(dot(vertex.viewDir, halfwayDir), 0.0f), shininess);
#else
    float3 reflectDirection = reflect(-lightDir, norm);
    float spec = pow(max(dot(vertex.viewDir, reflectDirection), 0.0f), shininess);
#endif
    float3 specular = (specularStrength * spec * lightColor);
    
    // Calculate attenuation
    float distance = length(pointLight.position.xyz - vertex.pixelPos.xyz);
    float attenuation = 1.0f / distance;
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    
    return (ambient + diffuse + specular) * pointLight.intensity;
}

float3 CalculateAllLights(Vertex vertex)
{
    float3 pixel_color = 0.0f;
    
    // Calculate Directional Lights
    uint num_directional_lights, stride;
    DirectionalLights.GetDimensions(num_directional_lights, stride);
    for (int i = 0; i < num_directional_lights; ++i)
    {
        if (DirectionalLights[i].intensity == 0.0f)
            continue;
        pixel_color += CalculateDirectionLight(vertex, DirectionalLights[i]);
    }

    // Calculate point lights
    uint numPointLights;
    PointLights.GetDimensions(numPointLights, stride);
    for (int j = 0; j < numPointLights; ++j)
    {
        if (PointLights[j].color.w == 0.0f)
            continue;
        pixel_color += CalculatePointLight(vertex, PointLights[j]);
    }
    
    return pixel_color;
}

Texture2D g_TextureDiffuse : register(t0);
Texture2D g_TextureEmissive : register(t1);

//=================
// Vertex Shader
//=================
Vertex VSMain(float3 position : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL, float4 color : COLOR)
{
    Vertex result;

    float4x4 mvpMatrix = mul(viewProjection, transform);
    
    result.position = mul(mvpMatrix, float4(position, 1.0f));
    result.pixelPos = mul(transform, float4(position, 1.0f));
    result.normal = TransformDirection(transform, normal);
    result.uv = uv;
    result.color = color.rgb;
    result.viewDir = normalize(viewPosition.xyz - result.pixelPos.xyz);

    return result;
}

//=================
// Pixel Shader
//=================
float4 PSMain(Vertex vertex) : SV_TARGET
{
    float4 diffuseTexture = g_TextureDiffuse.Sample(LinearWrap, vertex.uv);
    if (!all(diffuseTexture.rgb))
        diffuseTexture.rgb = vertex.color;
    float alpha = diffuseTexture.a;
    float4 emissive = g_TextureEmissive.Sample(LinearWrap, vertex.uv);
    
    vertex.color = diffuseTexture.rgb;
    vertex.color += emissive.rgb;
    
    float3 pixel_color = CalculateAllLights(vertex);
    
    return float4(pixel_color, alpha);
}