
struct VOut {
    float4 position : SV_Position;
    float4 color;
}

[shader("vertex")]
VOut vertexMain(uint vertexIndex : SV_VulkanVertexID) {

	//const array of positions for the triangle
	const float3 positions[3] = float3[3](
		float3(1.f,1.f, 0.0f),
		float3(-1.f,1.f, 0.0f),
		float3(0.f,-1.f, 0.0f)
	);

	//const array of colors for the triangle
	const float3 colors[3] = float3[3](
		float3(1.0f, 1.0f, 0.0f), //red
		float3(0.0f, 1.0f, 1.0f), //green
		float3(1.0f, 0.0f, 1.0f)  //blue
	);

    return VOut (float4(positions[vertexIndex],1), float4(colors[vertexIndex], 1.0));
}

[shader("fragment")]
float4 fragmentMain(VOut vertexIn) {
    return vertexIn.color;
}