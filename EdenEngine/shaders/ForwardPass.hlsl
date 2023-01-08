#include "Global.hlsli"
#ifndef ENABLE_PBR
#include "PhongLightingCommon.hlsli"
#else
#include "PBR.hlsl"
#endif

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
	float4 albedoTexture = g_AlbedoMap.Sample(LinearWrap, vertex.uv);
	// if there isn't any component that isn't 0, all the components are 0, 
	// so it means that it is rendering the _black texture_ from MeshSource, so set it to the own vertex color
	if (!any(albedoTexture.rgba)) 
		albedoTexture = float4(vertex.color, 1.0f);
	//float alpha = diffuseTexture.a;
#ifndef ENABLE_PBR
	float4 emissive = g_EmissiveMap.Sample(LinearWrap, vertex.uv);
    
    vertex.color = albedoTexture.rgb;
    vertex.color += emissive.rgb;
    
    float3 pixel_color = CalculateAllLights(vertex);
    
    return float4(pixel_color, 1.0f);
#else
	// Note that the albedo textures that come from artists are generally 
	// authored in sRGB space which is why we first convert them to 
	// linear space before using albedo in our lighting calculations.
	float3 albedo 	 = pow(albedoTexture.rgb, 2.2);
	float ao 		 = g_AOMap.Sample(LinearWrap, vertex.uv).r;
    float metallic   = g_MetallicRoughnessMap.Sample(LinearWrap, vertex.uv).r;
    float roughness  = g_MetallicRoughnessMap.Sample(LinearWrap, vertex.uv).g;
    float3 normalMap = g_NormalMap.Sample(LinearWrap, vertex.uv).rgb;

	return PBR(vertex, albedo, normalMap, metallic, roughness, ao);
#endif
}