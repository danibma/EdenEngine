RWTexture2D<float4> OutputTexture : register(u0);

cbuffer cb0
{
    float2 g_Resolution : packoffset(c0);
    float g_Time;
}

#define CIRCLE_SCALE 10.0f
#define COLOR_1_v3 float3(0.5f, 0.1f, 0.0f)
#define COLOR_2_v3 float3(0.95f, 0.2f, 0.8f)

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
#if 0
    float4 value = float4(0.0, 0.0, 0.0, 1.0);
    uint2 texelCoord = uint2(id.xy);
    
    value.x = float(texelCoord.x) / (g_MaxThreadIter.x);
    value.y = float(texelCoord.y) / (g_MaxThreadIter.y);
    OutputTexture[DTid.xy] = value;
#else
    // Normalized pixel coordinates (from 0 to 1)
    float2 uv = id.xy / g_Resolution.xy;
    uv -= 0.5f;
    uv.x *= g_Resolution.x / g_Resolution.y;
   
    float phi = atan2(uv.y, uv.x);
    float phi_func = 0.04 + 0.02 * sin(phi * 10.0);
    
    float dist = length(uv);
    dist = lerp(phi_func, phi_func + 1.0, dist);
    
    float frac_pattern = frac(CIRCLE_SCALE * (dist + 0.1f * g_Time));
    float circle_pattern = frac_pattern - smoothstep(0.95f, 1.0f, frac_pattern);
    
    float3 uvcol = 0.5 + 0.5 * cos(g_Time + 0.0f * uv.xyx + dist * 2.0f + float3(1, 2, 4));
    float3 uvcols = 0.25 + 0.25 * sin(g_Time + 0.0f * uv.xyx + dist * 2.0f + float3(1, 2, 4));
    
    float3 col = lerp(uvcols, uvcol, circle_pattern);
    OutputTexture[id.xy] = float4(col, 1.0f);
#endif
}