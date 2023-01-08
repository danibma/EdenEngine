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
	float4 baseColor		   : SV_TARGET0;
	float4 position			   : SV_TARGET1;
	float4 normal			   : SV_TARGET2;
	// r = metallic, g = roughness, b = AO
	float4 metallicRoughnessAO : SV_TARGET3; 
	float4 normalMap		   : SV_TARGET4;
};

//=================
// Pixel Shader
//=================
DeferredOutput PSMain(Vertex vertex)
{
	float4 diffuseTexture = g_AlbedoMap.Sample(LinearWrap, vertex.uv);
	if (!all(diffuseTexture.rgb))
		diffuseTexture.rgb = vertex.color;
	float alpha = diffuseTexture.a;

	float metallic = g_MetallicRoughnessMap.Sample(LinearWrap, vertex.uv).r;
	float roughness = g_MetallicRoughnessMap.Sample(LinearWrap, vertex.uv).g;
	float ao = g_AOMap.Sample(LinearWrap, vertex.uv).r;

	DeferredOutput output;
	output.baseColor = float4(diffuseTexture.rgb, alpha);
	output.position  = vertex.pixelPos;
	output.normal    = float4(vertex.normal, 1.0f);
	output.metallicRoughnessAO = float4(metallic, roughness, ao, 1.0f);
	output.normalMap = float4(g_NormalMap.Sample(LinearWrap, vertex.uv).rgb, 1.0f);

	return output;
}