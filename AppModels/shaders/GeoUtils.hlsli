struct VertexIn
{
    float3 Pos : POSITION;
    float3 Normal : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentU : TANGENTU;
    float3 BitangentU : BITANGENTU;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosW : POSITION;
    float3 NormalW : NORMALW;
    float2 TexC : TEXCOORD;
    float3 TangentW : TANGENTW;
    float3 BitangentW : BITANGENTW;
};
