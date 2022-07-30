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
    float3 light_color = float3(1.0f, 1.0f, 1.0f);
    
    // Ambient Light
    float ambient_strength = 0.1f;
    float3 ambient = ambient_strength * light_color * vertex.color;
    
    // Diffuse Light
    float3 norm = normalize(vertex.normal);
    float3 light_dir = normalize(directional_light.direction.xyz);
    float diffuse_color = max(dot(norm, light_dir), 0.0f);
    float3 diffuse = diffuse_color * light_color * vertex.color;
    
    // Specular Light
    float specular_strength = 0.1f;
    float shininess = 32.0f;
#if BLINN
    float3 halfway_dir = normalize(light_dir + vertex.view_dir);
    float spec = pow(max(dot(vertex.view_dir, halfway_dir), 0.0f), shininess);
#else
    float3 reflect_direction = reflect(-light_dir, norm);
    float spec = pow(max(dot(vertex.view_dir, reflect_direction), 0.0f), shininess);
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
StructuredBuffer<PointLight> PointLights;

float3 CalculatePointLight(Vertex vertex, PointLight point_light)
{
    // Lighting
    float3 light_color = point_light.color.rgb;
    
    // Ambient Light
    float ambient_strength = 0.1f;
    float3 ambient = ambient_strength * light_color * vertex.color;
    
    // Diffuse Light
    float3 norm = normalize(vertex.normal);
    float3 light_dir = normalize(point_light.position.xyz - vertex.pixel_pos.xyz);
    float diffuse_color = max(dot(norm, light_dir), 0.0f);
    float3 diffuse = diffuse_color * light_color * vertex.color;
    
    // Specular Light
    float specular_strength = 0.1f;
    float shininess = 32.0f;
#if BLINN
    float3 halfway_dir = normalize(light_dir + vertex.view_dir);
    float spec = pow(max(dot(vertex.view_dir, halfway_dir), 0.0f), shininess);
#else
    float3 reflect_direction = reflect(-light_dir, norm);
    float spec = pow(max(dot(vertex.view_dir, reflect_direction), 0.0f), shininess);
#endif
    float3 specular = (specular_strength * spec * light_color);
    
    // Calculate attenuation
    float distance = length(point_light.position.xyz - vertex.pixel_pos.xyz);
    float attenuation = 1.0f / distance;
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    
    return (ambient + diffuse + specular) * point_light.intensity;
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
    uint num_point_lights;
    PointLights.GetDimensions(num_point_lights, stride);
    for (int j = 0; j < num_point_lights; ++j)
    {
        if (PointLights[j].color.w == 0.0f)
            continue;
        pixel_color += CalculatePointLight(vertex, PointLights[j]);
    }
    
    return pixel_color;
}

Texture2D g_textureDiffuse : register(t0);
Texture2D g_textureEmissive : register(t1);

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
    float4 diffuse_texture = g_textureDiffuse.Sample(LinearWrap, vertex.uv);
    if (!all(diffuse_texture.rgb))
        diffuse_texture.rgb = vertex.color;
    float alpha = diffuse_texture.a;
    float4 emissive = g_textureEmissive.Sample(LinearWrap, vertex.uv);
    
    vertex.color = diffuse_texture.rgb;
    vertex.color += emissive.rgb;
    
    float3 pixel_color = CalculateAllLights(vertex);
    
    return float4(pixel_color, alpha);
}