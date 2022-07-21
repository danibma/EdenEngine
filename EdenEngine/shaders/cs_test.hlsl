RWTexture2D<float4> OutputTexture : register(u0);

cbuffer cb0
{
    float4 g_MaxThreadIter : packoffset(c0);
}

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
    float4 value = float4(0.0, 0.0, 0.0, 1.0);
    uint2 texelCoord = uint2(DTid.xy);
    
    value.x = float(texelCoord.x) / (g_MaxThreadIter.x);
    value.y = float(texelCoord.y) / (g_MaxThreadIter.y);
    OutputTexture[DTid.xy] = value;
}