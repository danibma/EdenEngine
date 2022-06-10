struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

cbuffer SceneData : register(b0)
{
    float4x4 view;
    float4x4 view_projection;
    float4 view_position;
};

cbuffer Transform : register(b1)
{
    float4x4 transform;
};

cbuffer LightColor : register(b2)
{
    float4 light_color;
};

//=================
// Vertex Shader
//=================
PSInput VSMain(float3 position : POSITION, float2 uv : TEXCOORD, float3 normal : NORMAL, float4 color : COLOR)
{
    PSInput result;

    float4x4 mvpMatrix = mul(view_projection, transform);
    
    result.position = mul(mvpMatrix, float4(position, 1.0f));
    result.color = color;

    return result;
}

//=================
// Pixel Shader
//=================
float4 PSMain(PSInput input) : SV_TARGET
{
    float4 object_color = light_color;

    return object_color * 3;
}