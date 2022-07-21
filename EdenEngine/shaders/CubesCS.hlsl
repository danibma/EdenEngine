// This is a compute shader test
// Based on https://www.shadertoy.com/view/NsGfDW

RWTexture2D<float4> OutputTexture : register(u0);

cbuffer RenderingInfo
{
    float2 g_Resolution : packoffset(c0);
    float g_Time;
}

#define H(p) sin(g_Time * 0.5 + frac(sin(dot(p, float2(12.9898, 78.233))) * 43758.5453) * 10.0) * 0.5 + 0.5

[numthreads(8, 8, 1)]
void CSMain(uint3 id : SV_DispatchThreadID)
{
    float2 r = g_Resolution.xy;
    float2 u = (id.xy + id.xy - r) / r.y * 5; //camera scaling
    float2 l = frac(u); //cell local coordinates
    float2 s, z;
    u = floor(u); //cell id
    
    float h = H(u); //cell "height"
    float d = 1;
    float n, f;
    
    for (int i = 0; i < 9; i++)   //calculate distance field to 8 neighbors cells
    {
        s = float2(i / 3, i % 3) - 1.; //cell id shift
        z = abs(l - 0.5 - s) - 0.5; //the distance to the neighbor cell divided by the height difference with the current cell
        n = (length(max(z, 0)) + min(max(z.x, z.y), 0)) / max(H(u+s) - h, 0.05) / 2; //more difference -> less distance -> darker shadow 
        f = max(0, 1 - abs(n - d) / 0.4); //smooth min of distance
        d = (i != 4) ? min(d, n) - 0.1 * f * f : d; //skiping current cell
    }
    OutputTexture[id.xy] = lerp(float4(0.4, 0.5, 0.6, 1), float4(1, 0.9, 0.6, 1), h) + min(d, 0.2) - 0.3;
}