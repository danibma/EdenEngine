#include "Global.hlsli"

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

struct DeferredOutput
{
	float4 baseColor : SV_TARGET0;
	float4 position  : SV_TARGET1;
	float4 normal    : SV_TARGET2;
};

Texture2D g_TextureDiffuse : register(t0);

//=================
// Pixel Shader
//=================
DeferredOutput PSMain(Vertex vertex)
{
	float4 diffuseTexture = g_TextureDiffuse.Sample(LinearWrap, vertex.uv);
	if (!all(diffuseTexture.rgb))
		diffuseTexture.rgb = vertex.color;
	float alpha = diffuseTexture.a;

	DeferredOutput output;
	output.baseColor = float4(diffuseTexture.rgb, alpha); // color + specular
	output.position  = vertex.pixelPos;
	output.normal    = float4(vertex.normal, 1.0f);

	return output;
}