#include "Common.hlsli"
#include "GeoUtils.hlsli"

ConstantBuffer<ObjectConstants> ObjectConstantsCB : register(b0);
ConstantBuffer<PassConstants> PassConstantsCB : register(b1);

VertexOut main(VertexIn vin) {
    VertexOut vout;
    
    vout.TexC = vin.TexC;
    vout.Norm = vin.Norm;
    float4 posW = mul(ObjectConstantsCB.ModelMatrix, float4(vin.Pos, 1.0f));
    vout.PosW = posW.xyz;
    vout.PosH = mul(PassConstantsCB.LightViewProj, posW);
    
    return vout;
}