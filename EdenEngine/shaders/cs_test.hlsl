RWTexture2D<float4> OutputTexture : register(u0);

cbuffer cb0
{
    float4 g_MaxThreadIter : packoffset(c0);
    float g_Time;
}

[numthreads(8, 8, 1)]
void CSMain(uint3 DTid : SV_DispatchThreadID)
{
#if 0
    float4 value = float4(0.0, 0.0, 0.0, 1.0);
    uint2 texelCoord = uint2(DTid.xy);
    
    value.x = float(texelCoord.x) / (g_MaxThreadIter.x);
    value.y = float(texelCoord.y) / (g_MaxThreadIter.y);
    OutputTexture[DTid.xy] = value;
#else
    // Normalized pixel coordinates (from 0 to 1)
    float2 texelCoord = DTid.xy / g_MaxThreadIter.xy;
    
    // Calculate the to center distance
    float d = length(texelCoord) * 2.0;
    
    // Calculate the ripple time
    float t = d * d * 25.0 - g_Time * 3.0;
    
    // Calculate the ripple thickness
    d = (cos(t) * 0.5 + 0.5) * (1.0 - d);
    
    // Time varying pixel color
    float3 col = 0.5 + 0.5 * cos(t / 20.0 + texelCoord.xyx + float3(0.0, 2.0, 4.0));

    // Set the output color to rgb channels and the thickness to alpha channel
    // AO is automatically calculated
    OutputTexture[DTid.xy] = float4(col, d);
#endif
}