float4 main(uint vertexId : SV_VertexID) : SV_Position
{
    float2 tex = float2(uint2(vertexId, vertexId << 1) & 2);
    return float4(lerp(float2(-1.0f, -1.0f), float2(1.0f, 1.0f), tex), 0.0f, 1.0f);
}