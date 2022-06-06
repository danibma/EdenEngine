//=================
// Directional Light
//=================
struct DirectionalLight
{
    float4 direction;
};

float4 CalculateDirectionLight(float4 objectColor, float4 positionModelMatrix, float3 normal, DirectionalLight directionalLight)
{
    // Lighting
    float3 lightColor = float3(1.0f, 1.0f, 1.0f);
    float3 fragPos = float3(positionModelMatrix.xyz);
    
    // Ambient Light
    float ambientStrength = 0.1f;
    float4 ambient = float4(ambientStrength * lightColor, 1.0f) * objectColor;
    
    // Diffuse Light
    float3 norm = normalize(normal);
    //float3 lightDirection = normalize(lightPos - fragPos);
    float3 lightDir = normalize(directionalLight.direction.xyz);
    float diffuseColor = max(dot(norm, lightDir), 0.0f);
    float4 diffuse = float4(diffuseColor * lightColor, 1.0f) * objectColor;
    
    // Specular Light
    float specularStrength = 0.1f;
    float3 viewDirection = normalize(-fragPos);
    float3 reflectDirection = reflect(-lightDir, norm);
    float shininess = 32.0f;
    float spec = pow(max(dot(viewDirection, reflectDirection), 0.0f), shininess);
    float4 specular = float4((specularStrength * spec * lightColor), 1.0f);
    
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

float4 CalculatePointLight(float4 objectColor, float4 positionModelMatrix, float3 normal, PointLight pointLight)
{
     // Lighting
    float3 lightColor = pointLight.color.rgb;
    float3 fragPos = float3(positionModelMatrix.xyz);
    
    // Ambient Light
    float ambientStrength = 0.1f;
    float4 ambient = float4(ambientStrength * lightColor, 1.0f) * objectColor;
    
    // Diffuse Light
    float3 norm = normalize(normal);
    float3 lightDir = normalize(pointLight.position.xyz - fragPos);
    //float3 lightDir = normalize(lightDirection);
    float diffuseColor = max(dot(norm, lightDir), 0.0f);
    float4 diffuse = float4(diffuseColor * lightColor, 1.0f) * objectColor;
    
    // Specular Light
    float specularStrength = 0.1f;
    float3 viewDirection = normalize(-fragPos);
    float3 reflectDirection = reflect(-lightDir, norm);
    float shininess = 32.0f;
    float spec = pow(max(dot(viewDirection, reflectDirection), 0.0f), shininess);
    float4 specular = float4((specularStrength * spec * lightColor), 1.0f);
    
    // Calculate attenuation
    float distance = length(pointLight.position.xyz - fragPos);
    float attenuation = 1.0f / (pointLight.constant_value + pointLight.linear_value * distance + pointLight.quadratic_value * (distance * distance));
    ambient  *= attenuation;
    diffuse  *= attenuation;
    specular *= attenuation;
    
    return ambient + diffuse + specular;
}