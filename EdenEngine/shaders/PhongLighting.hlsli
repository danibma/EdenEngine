#pragma once
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

float3 CalculateDirectionLight(const float3 object_color, float4 frag_pos, const float3 view_dir, const float3 normal, DirectionalLight directional_light)
{
    // Lighting
    float3 light_color = float3(1.0f, 1.0f, 1.0f);
    
    // Ambient Light
    float ambient_strength = 0.1f;
    float3 ambient = ambient_strength * light_color * object_color;
    
    // Diffuse Light
    float3 norm = normalize(normal);
    float3 light_dir = normalize(directional_light.direction.xyz);
    float diffuse_color = max(dot(norm, light_dir), 0.0f);
    float3 diffuse = diffuse_color * light_color * object_color;
    
    // Specular Light
    float specular_strength = 0.1f;
    float shininess = 32.0f;
#if BLINN
    float3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(max(dot(view_dir, halfway_dir), 0.0f), shininess);
#else
    float3 reflect_direction = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_direction), 0.0f), shininess);
#endif
    float3 specular = (specular_strength * spec * light_color);

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

float3 CalculatePointLight(const float3 object_color, float4 frag_pos, const float3 view_dir, const float3 normal, PointLight point_light)
{
    // Lighting
    float3 light_color = point_light.color.rgb;
    
    // Ambient Light
    float ambient_strength = 0.1f;
    float3 ambient = ambient_strength * light_color * object_color;
    
    // Diffuse Light
    float3 norm = normalize(normal);
    float3 light_dir = normalize(point_light.position.xyz - frag_pos.xyz);
    float diffuse_color = max(dot(norm, light_dir), 0.0f);
    float3 diffuse = diffuse_color * light_color * object_color;
    
    // Specular Light
    float specular_strength = 0.1f;
    float shininess = 32.0f;
#if BLINN
    float3 halfway_dir = normalize(light_dir + view_dir);
    float spec = pow(max(dot(view_dir, halfway_dir), 0.0f), shininess);
#else
    float3 reflect_direction = reflect(-light_dir, norm);
    float spec = pow(max(dot(view_dir, reflect_direction), 0.0f), shininess);
#endif
    float3 specular = (specular_strength * spec * light_color);
    
    // Calculate attenuation
    float distance = length(point_light.position.xyz - frag_pos.xyz);
    float attenuation = 1.0f / distance;
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    
    return (ambient + diffuse + specular) * point_light.intensity;
}

float3 CalculateAllLights(Vertex vertex, StructuredBuffer<DirectionalLight> DirectionalLights, StructuredBuffer<PointLight> PointLights)
{
    float3 pixel_color = 0.0f;
    
    // Calculate Directional Lights
    uint num_directional_lights, stride;
    DirectionalLights.GetDimensions(num_directional_lights, stride);
    for (int i = 0; i < num_directional_lights; ++i)
    {
        if (DirectionalLights[i].intensity == 0.0f)
            continue;
        pixel_color += CalculateDirectionLight(vertex.color, vertex.pixel_pos, vertex.view_dir, vertex.normal, DirectionalLights[i]);
    }

    // Calculate point lights
    uint num_point_lights;
    PointLights.GetDimensions(num_point_lights, stride);
    for (int j = 0; j < num_point_lights; ++j)
    {
        if (PointLights[j].color.w == 0.0f)
            continue;
        pixel_color += CalculatePointLight(vertex.color, vertex.pixel_pos, vertex.view_dir, vertex.normal, PointLights[j]);
    }
    
    return pixel_color;
}