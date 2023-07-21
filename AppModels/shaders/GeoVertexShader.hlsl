#include "Common.hlsli"
#include "GeoUtils.hlsli"

ConstantBuffer<ObjectConstants> ObjectConstantsCB : register(b0);
ConstantBuffer<PassConstants> PassConstantsCB :register(b1);

VertexOut main(VertexIn vin)
{
    VertexOut vout;
    
    vout.Norm = normalize(mul((float3x3)ObjectConstantsCB.ModelMatrixInvTrans, vin.Norm));
    vout.TexC = vin.TexC;
    vout.TangentW = mul((float3x3) ObjectConstantsCB.ModelMatrix, vin.TangentU);
    
    float4 posW = mul(ObjectConstantsCB.ModelMatrix, float4(vin.Pos, 1.0f));
    vout.PosW = posW.xyz;

    vout.PosH = mul(PassConstantsCB.ViewProj, posW);
    
    return vout;
}