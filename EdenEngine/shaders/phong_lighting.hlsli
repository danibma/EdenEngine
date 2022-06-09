//=================
// Directional Light
//=================
struct DirectionalLight
{
    float4 direction;
};

float4 CalculateDirectionLight(const float4 object_color, float4 frag_pos, const float3 view_dir, const float3 normal, DirectionalLight directional_light)
{
    // Lighting
    float3 light_color = float3(1.0f, 1.0f, 1.0f);
    
    // Ambient Light
    float ambient_strength = 0.1f;
    float4 ambient = float4(ambient_strength * light_color, 1.0f) * object_color;
    
    // Diffuse Light
    float3 norm = normalize(normal);
    float3 light_dir = normalize(directional_light.direction.xyz);
    float diffuse_color = max(dot(norm, light_dir), 0.0f);
    float4 diffuse = float4(diffuse_color * light_color, 1.0f) * object_color;
    
    // Specular Light
    float specular_strength = 0.1f;
    float3 reflect_direction = reflect(-light_dir, norm);
    float shininess = 32.0f;
    float spec = pow(max(dot(view_dir, reflect_direction), 0.0f), shininess);
    float4 specular = float4((specular_strength * spec * light_color), 1.0f);

    return ambient + diffuse + specular;
}

//=================
// Point Light
//=================
struct PointLight
{
    float4 color;
    float4 position;
    float constant_value;
    float linear_value;
    float quadratic_value;
    float padding; // no use
};

float4 CalculatePointLight(const float4 object_color, float4 frag_pos, const float3 view_dir, const float3 normal, PointLight point_light)
{
     // Lighting
    float3 light_color = point_light.color.rgb;
    
    // Ambient Light
    float ambient_strength = 0.1f;
    float4 ambient = float4(ambient_strength * light_color, 1.0f) * object_color;
    
    // Diffuse Light
    float3 norm = normalize(normal);
    float3 light_dir = normalize(point_light.position.xyz - frag_pos.xyz);
    float diffuse_color = max(dot(norm, light_dir), 0.0f);
    float4 diffuse = float4(diffuse_color * light_color, 1.0f) * object_color;
    
    // Specular Light
    float specular_strength = 0.1f;
    float3 reflect_direction = reflect(-light_dir, norm);
    float shininess = 32.0f;
    float spec = pow(max(dot(view_dir, reflect_direction), 0.0f), shininess);
    float4 specular = float4((specular_strength * spec * light_color), 1.0f);
    
    // Calculate attenuation
    float distance = length(point_light.position.xyz - frag_pos.xyz);
    float attenuation = 1.0f / (point_light.constant_value + point_light.linear_value * distance + point_light.quadratic_value * (distance * distance));
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    
    return ambient + diffuse + specular;
}