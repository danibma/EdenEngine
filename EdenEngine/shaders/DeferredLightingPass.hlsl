#include "Global.hlsli"
#ifndef ENABLE_PBR
#include "PhongLightingCommon.hlsli"
#else
#include "PBR.hlsl"
#endif

Texture2D g_TexturePosition			   : register(t0);
Texture2D g_TextureNormal			   : register(t1);
Texture2D g_TextureBaseColor		   : register(t2);
Texture2D g_TextureMetallicRoughnessAO : register(t3);
Texture2D g_TextureNormalMap		   : register(t4);

Vertex VSMain(float2 position : POSITION, float2 uv : TEXCOORD)
{
	Vertex result;
	result.position = float4(position.x, position.y, 0.0f, 1.0f);
	result.uv = -uv;
	return result;
}

float4 PSMain(Vertex vertex) : SV_TARGET
{
	float3 baseColor = g_TextureBaseColor.Sample(LinearWrap, vertex.uv).rgb;
	float  alpha  	 = g_TextureBaseColor.Sample(LinearWrap, vertex.uv).a;
	float3 position  = g_TexturePosition.Sample(LinearWrap, vertex.uv).rgb;
	float3 normal    = g_TextureNormal.Sample(LinearWrap, vertex.uv).rgb;

	if (alpha == 0.0f)
		discard;

	vertex.color 	= baseColor;
	vertex.pixelPos = float4(position, 1.0f);
	vertex.normal   = normal;
	vertex.viewDir  = normalize(viewPosition.xyz - vertex.pixelPos.xyz);
#ifndef ENABLE_PBR
	float3 pixelColor = CalculateAllLights(vertex);
	return float4(pixelColor, alpha);
#else
	// Note that the albedo textures that come from artists are generally 
	// authored in sRGB space which is why we first convert them to 
	// linear space before using albedo in our lighting calculations.
	float3 albedo = pow(baseColor, 2.2);
	float3 metallicRoughnessAO = g_TextureMetallicRoughnessAO.Sample(LinearWrap, vertex.uv).rgb;
	float metallic = metallicRoughnessAO.r;
	float roughness = metallicRoughnessAO.g;
	float ao = metallicRoughnessAO.b;
	float3 normalMap = g_TextureNormalMap.Sample(LinearWrap, vertex.uv).rgb;

	return PBR(vertex, albedo, normalMap, metallic, roughness, ao);
#endif
}