#include "Global.hlsli"
#include "PhongLightingCommon.hlsli"

Texture2D g_TexturePosition  : register(t0);
Texture2D g_TextureNormal    : register(t1);
Texture2D g_TextureBaseColor : register(t2);

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

	float3 pixelColor = CalculateAllLights(vertex);

	return float4(pixelColor, alpha);
}