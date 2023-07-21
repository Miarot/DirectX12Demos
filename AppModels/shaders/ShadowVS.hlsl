#include "Common.hlsli"
#include "GeoUtils.hlsli"

ConstantBuffer<ObjectConstants> ObjectConstantsCB : register(b0);
ConstantBuffer<PassConstants> PassConstantsCB : register(b1);

cbuffer LightConstants : register(b3)
{
    int LightIndex;
};

struct ShadowVertextOut {
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD;
};

ShadowVertextOut main(VertexIn vin)
{
    ShadowVertextOut vout;
    
    vout.TexC = vin.TexC;
    
    float4 posW = mul(ObjectConstantsCB.ModelMatrix, float4(vin.Pos, 1.0f));
    vout.PosH = mul(PassConstantsCB.Lights[LightIndex].LightViewProj, posW);
    
    return vout;
}