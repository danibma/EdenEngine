#include "Global.hlsli"

cbuffer ObjectID
{
    uint ObjectID;
};

//=================
// Vertex Shader
//=================
Vertex VSMain(float3 position : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL, float4 color : COLOR)
{
    float4x4 mvp_matrix = mul(view_projection, transform);
    Vertex result;
    result.position = mul(mvp_matrix, float4(position, 1.0f));
    result.uv = uv;
    result.color = (float3)ObjectID;

    return result;
}

//=================
// Pixel Shader
//=================
float4 PSMain(Vertex vertex) : SV_TARGET
{
    return float4(vertex.color, 1.0f);
}