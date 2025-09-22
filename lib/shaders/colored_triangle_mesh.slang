
struct Vertex {
	float3 position;
	float uv_x;
	float3 normal;
	float uv_y;
	float4 color;
}

struct constants {
	float4x4 render_matrix;
	Vertex* vertexBuffer;
}

struct VOut {
    float4 position : SV_Position;
    float4 color;
    float2 uv;
}

[shader("vertex")]
VOut vertexMain(uint vertexIndex : SV_VulkanVertexID,
					[vk::push_constant] uniform constants uniforms) {
	Vertex v = uniforms.vertexBuffer[vertexIndex];

    VOut ret;
    ret.position = mul(uniforms.render_matrix, float4(v.position, 1));
    ret.color = v.color;
    ret.uv = float2(v.uv_x, v.uv_y);
	return ret;
}