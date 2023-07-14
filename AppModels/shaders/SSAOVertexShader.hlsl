#include "Common.hlsli"

static const float2 gTexCoords[6] = {
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosV : POSITION;
    float2 TexC : TEXCOORD;
};

ConstantBuffer<PassConstants> PassConstantsCB : register(b0);

VertexOut main(uint vertexId : SV_VertexID)
{
    VertexOut vout;
    
    vout.TexC = gTexCoords[vertexId];
    vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);
    
    // from NDC to coordinates on near plane (or far plane if reversed depth)
    float4 posV = mul(PassConstantsCB.ProjInv, vout.PosH);
    vout.PosV = posV.xyz / posV.w;
    
    return vout;
}