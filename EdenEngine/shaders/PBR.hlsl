#include "Global.hlsli"

// Convert tangent-normals to world space
// Normal vectors in a normal map are expressed in tangent space where normals always point 
// roughly in the positive z direction. Tangent space is a space that's local to the surface of a triangle
// trick from https://learnopengl.com/code_viewer_gh.php?code=src/6.pbr/1.2.lighting_textured/1.2.pbr.fs
float3 GetNormalFromMap(Vertex vertex, float3 normalMap)
{
	float3 tangentNormal = normalMap * 2.0 - 1.0;

	float3 Q1  = ddx(vertex.pixelPos).rgb;
	float3 Q2  = ddy(vertex.pixelPos).rgb;
	float2 st1 = ddx(vertex.uv);
	float2 st2 = ddy(vertex.uv);

	float3 N     = vertex.normal;
	float3 T     = normalize(Q1 * st2.g - Q2 * st1.g);
	float3 B   	 = -normalize(cross(N, T));
	// Tangent, bitangent, normal matrix
	float3x3 TBN = float3x3(T, B, N);

	return normalize(mul(TBN, tangentNormal));
}

float DistributionGGX(float3 N, float3 H, float roughness)
{
	float a      = roughness * roughness;
	float a2     = a * a;
	float NdotH  = max(dot(N, H), 0.0);
	float NdotH2 = NdotH * NdotH;
	
	float num   = a2;
	float denom = (NdotH2 * (a2 - 1.0) + 1.0);
	denom = PI * denom * denom;
	
	return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0);
	float k = (r * r) / 8.0;

	float num   = NdotV;
	float denom = NdotV * (1.0 - k) + k;
	
	return num / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0);
	float NdotL = max(dot(N, L), 0.0);
	float ggx2  = GeometrySchlickGGX(NdotV, roughness);
	float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
	return ggx1 * ggx2;
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

//=================
// Point Light
//=================
struct PointLight
{
	float4 color;
	float4 position;
	float intensity;
};
StructuredBuffer<PointLight> PointLights;

float4 PBR(Vertex vertex, float3 albedo, float3 normalMap, float metallic, float roughness, float ao)
{
	//ao = 1.0f;
	//albedo = float3(1, 0, 0);
	//metallic = 1.0f;
	//roughness = 1.0f;

	//float3 N = GetNormalFromMap(vertex, normalMap);
	float3 N = normalize(vertex.normal);
	float3 V = vertex.viewDir;

	float3 F0 = (float3)0.04f; 
	F0 = lerp(F0, albedo, metallic);

	uint numPointLights, stride;
	PointLights.GetDimensions(numPointLights, stride);

	float3 Lo = (float3)0.0f;
	for (int i = 0; i < numPointLights; ++i)
	{
		// calculate per-light radiance
		float3 L = normalize(PointLights[i].position.xyz - vertex.pixelPos.xyz);
		float3 H = normalize(V + L);
		float distance = length(PointLights[i].position.xyz - vertex.pixelPos.xyz);
		float attenuation = 1.0f / (distance * distance);
		float3 radiance = PointLights[i].color.rgb * attenuation;

		// cook torrance brdf
		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);

		float3 kS = F;
		float3 kD = (float3)1.0f - kS;
		kD *= 1.0f - metallic;

		float3 numerator = NDF * G * F;
		float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
		float3 specularf = numerator / denominator;
		float specular = specularf.x;

		// add to outgoing radiance Lo
		float NdotL = max(dot(N, L), 0.0f);
		Lo += ((kD * albedo / PI + specular) * radiance * NdotL) * PointLights[i].intensity;
	}

	float3 ambient = (float3)0.3f * albedo;
	float3 color = ambient + Lo;

	return float4(color, 1.0f);
	//return float4(metallic, roughness, 1.0f, 1.0f);
}
