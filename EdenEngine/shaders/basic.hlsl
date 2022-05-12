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
    float4x4 modelMatrix;
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
    result.positionModel = mul(modelMatrix, float4(position, 1.0f));
    result.normal = normal;
    result.uv = uv;

    return result;
}

float4 PSMain(PSInput input) : SV_TARGET
{
    float4 objectColor = g_textureDiffuse.Sample(g_samplerDiffuse, input.uv);
    
    // Lighting
    float3 lightColor = float3(1.0f, 1.0f, 1.0f);
    float3 lightPos = lightPosition;
    
    // Ambient Light
    float ambientStrength = 0.1f;
    float4 ambient = float4(ambientStrength * lightColor, 1.0f);
    
    // Diffuse Light
    float3 norm = normalize(input.normal);
    float3 lightDirection = normalize(lightPos - float3(input.positionModel.xyz));
    float diffuseColor = max(dot(norm, lightDirection), 0.0f);
    float4 diffuse = float4(diffuseColor * lightColor, 1.0f);
    
    float4 pixelColor = (ambient + diffuse) * objectColor;
    
    return pixelColor;
}