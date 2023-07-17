#include "Common.hlsli"
#include "GeoUtils.hlsli"

ConstantBuffer<PassConstants> PassConstantsCB : register(b1);
ConstantBuffer<MaterialConstants> MaterialConstantsCB : register(b2);

Texture2D Texture : register(t0);
SamplerState LinearWrapSampler : register(s0);

void main(VertexOut pin)
{
    float4 diffuseAlbedo = MaterialConstantsCB.DiffuseAlbedo * Texture.Sample(LinearWrapSampler, pin.TexC);
    
    #ifdef ALPHA_TEST
        clip(diffuseAlbedo.a - 0.1f);
    #endif
    
    return;
}