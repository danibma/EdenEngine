#include "Global.hlsli"
#include "PhongLightingCommon.hlsli"

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