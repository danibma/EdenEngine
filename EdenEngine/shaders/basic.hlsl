struct PSInput
{
    float4 position : SV_POSITION;
    float4 positionModel : Position;
    float2 uv : TEXCOORD;
    float3 normal : NORMAL;
};

cbuffer SceneData : register(b0)
{
    float4x4 mvpMatrix;
    float4x4 modelViewMatrix;
    float4x4 normalMatrix;
    float3 lightPosition;
};

Texture2D g_textureDiffuse : register(t0);
SamplerState g_samplerDiffuse : register(s0);

// NOTE(Daniel): Loading normal maps correctly, just not using them because we have no light
Texture2D g_textureNormal : register(t1);
SamplerState g_samplerNormal : register(s1);

PSInput VSMain(float3 position : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL)
{
    PSInput result;

    result.position = mul(mvpMatrix, float4(position, 1.0f));
    result.positionModel = mul(modelViewMatrix, float4(position, 1.0f));
    result.normal = float3(mul(normalMatrix, float4(normal, 1.0f)).xyz);
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 diffuseTexture = g_textureDiffuse.Sample(g_samplerDiffuse, input.uv);
    float4 normalTexture = g_textureNormal.Sample(g_samplerNormal, input.uv);
    
    // Lighting
    float3 lightColor = float3(1.0f, 1.0f, 1.0f);
    float3 lightPos = lightPosition;
    float3 fragPos = float3(input.positionModel.xyz);
    
    // Ambient Light
    float ambientStrength = 0.1f;
    float4 ambient = float4(ambientStrength * lightColor, 1.0f) * diffuseTexture;
    
    // Diffuse Light
    float3 norm = normalize(input.normal);
    float3 lightDirection = normalize(lightPos - fragPos);
    float diffuseColor = max(dot(norm, lightDirection), 0.0f);
    float4 diffuse = float4(diffuseColor * lightColor, 1.0f) * diffuseTexture;
    
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