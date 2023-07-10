#include "Common.hlsli"
#include "GeoUtils.hlsli"

ConstantBuffer<PassConstants> PassConstantsCB : register(b1);
ConstantBuffer<MaterialConstants> MaterilaConstantsCB : register(b2);

Texture2D Texture : register(t0);
SamplerState LinearWrapSampler : register(s0);

float4 main(VertexOut pin) : SV_Target
{
    float4 diffeseAlbedo = MaterilaConstantsCB.DiffuseAlbedo * Texture.Sample(LinearWrapSampler, pin.TexC);
    
    #ifdef ALPHA_TEST
        clip(diffeseAlbedo.a - 0.1f);
    #endif
    
    pin.Norm = normalize(pin.Norm);
    float3 normV = mul((float3x3)PassConstantsCB.View, pin.Norm);
    
    return float4(normV, 0.0f);
}