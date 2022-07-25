#include "Global.hlsli"

Texture2D<float4> SrcTexture : register(t0);
RWTexture2D<float4> DstTexture : register(u0);

cbuffer TexelSize : register(b0)
{
    float2 TexelSize; // 1.0 / destination dimension
}

[numthreads(8, 8, 1)]
void CSMain(uint2 ThreadID : SV_DispatchThreadID)
{
    //DTid is the thread ID * the values from numthreads above and in this case correspond to the pixels location in number of pixels.
	//As a result texcoords (in 0-1 range) will point at the center between the 4 pixels used for the mipmap.
    float2 texcoords = TexelSize * (ThreadID + 0.5);

	//The samplers linear interpolation will mix the four pixel values to the new pixels color
    float4 color = SrcTexture.SampleLevel(LinearClamp, texcoords, 0);

	//Write the final color into the destination texture.
    DstTexture[ThreadID] = float4(color.rgba);
}
