cbuffer Transform
{
	float4x4 transform;
};

cbuffer SceneData
{
	float4x4 viewProjection;
};

SamplerState LinearWrap : register(s0);

Texture2D g_AlbedoMap 	         : register(t0);
Texture2D g_EmissiveMap          : register(t1);

struct Vertex
{
	float4 position : SV_POSITION;
	float2 uv		: TEXCOORD;
	float3 color	: COLOR;
};

//=================
// Vertex Shader
//=================
Vertex VSMain(float3 position : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL, float4 color : COLOR)
{
	Vertex result;

	float4x4 mvpMatrix = mul(viewProjection, transform);

	result.position = mul(mvpMatrix, float4(position, 1.0f));
	result.uv = uv;
	result.color = color.rgb;

	return result;
}

//=================
// Pixel Shader
//=================
float4 PSMain(Vertex vertex) : SV_TARGET
{
	float4 albedoTexture = g_AlbedoMap.Sample(LinearWrap, vertex.uv);
	// if there isn't any component that isn't 0, all the components are 0, 
	// so it means that it is rendering the _black texture_ from MeshSource, so set it to the own vertex color
	if (!any(albedoTexture.rgba))
		albedoTexture = float4(vertex.color, 1.0f);
	//float alpha = diffuseTexture.a;

	float4 emissive = g_EmissiveMap.Sample(LinearWrap, vertex.uv);

	vertex.color = albedoTexture.rgb;
	vertex.color += emissive.rgb;

	return float4(vertex.color, 1.0f);
}