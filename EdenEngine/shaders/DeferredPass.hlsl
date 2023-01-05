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
	float4 position : SV_TARGET0;
	float4 normals : SV_TARGET1;
	float4 albedos : SV_TARGET2;
	float4 specular : SV_TARGET3;
};

//=================
// Pixel Shader
//=================
DeferredOutput PSMain(Vertex vertex)
{
	DeferredOutput output;
	output.position = (float4)0.3f;
	output.normals  = (float4)0.6f;
	output.albedos  = (float4)0.8f;
	output.specular = (float4)1.0f;

	return output;
}