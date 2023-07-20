struct VertexIn
{
    float3 Pos : POSITION;
    float3 Norm : NORM;
    float2 TexC : TEXCOORD;
    float3 TangentU : TANGENTU;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosW : POSITION;
    float3 Norm : NORM;
    float2 TexC : TEXCOORD;
    float3 TangentU : TANGENTU;
};
