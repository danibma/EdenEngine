struct Vertex
{
	float4 position : SV_Position;
	float4 color    : OUT_COLOR;
};

//=================
// Vertex Shader
//=================
Vertex VSMain(float3 position : POSITION, float3 color : IN_COLOR)
{
	Vertex result;
	result.position = float4(position, 1.0f);
	result.color    = float4(color, 1.0f);
	return result;
}

//=================
// Pixel Shader
//=================
float4 PSMain(Vertex vertex) : SV_TARGET
{
	return vertex.color;
}