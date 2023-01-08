#include "Global.hlsli"

// Convert tangent-normals to world space
// Normal vectors in a normal map are expressed in tangent space where normals always point 
// roughly in the positive z direction. Tangent space is a space that's local to the surface of a triangle
// trick from https://learnopengl.com/code_viewer_gh.php?code=src/6.pbr/1.2.lighting_textured/1.2.pbr.fs
float3 GetNormalFromMap(Vertex vertex, float3 normalMap)
{
	float3 tangentNormal = normalMap * 2.0 - 1.0;

	float3 Q1  = ddx(vertex.position).rgb;
	float3 Q2  = ddy(vertex.position).rgb;
	float2 st1 = ddx(vertex.uv);
	float2 st2 = ddy(vertex.uv);

	float3 N     = normalize(vertex.normal);
	float3 T     = normalize(Q1 * st2.g - Q2 * st1.g);
	float3 B   	 = -normalize(cross(N, T));
	float3x3 TBN = float3x3(T, B, N);

	return normalize(mul(TBN, tangentNormal));
}

float4 PBR(Vertex vertex, float3 albedo, float3 normalMap, float metallic, float roughness, float ao)
{
	float3 normal = GetNormalFromMap(vertex, normalMap);

	return (float4)(roughness);
}
