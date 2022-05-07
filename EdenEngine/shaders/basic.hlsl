struct PSInput
{
    float4 position : SV_Position;
    float4 color : Color;
};

PSInput VSMain(float4 position : Position, float4 color : Color)
{
    PSInput result;

    result.position = position;
    result.color = color;

    return result;
}

float4 PSMain(PSInput input) : SV_Target
{
    return input.color;
}
