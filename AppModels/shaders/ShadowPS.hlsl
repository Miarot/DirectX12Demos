#include "Common.hlsli"
#include "GeoUtils.hlsli"

ConstantBuffer<MaterialConstants> MaterialConstantsCB : register(b2);

Texture2D Texture : register(t0);
SamplerState LinearWrapSampler : register(s0);

struct ShadowVertextOut
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD;
};

void main(ShadowVertextOut pin)
{
    float4 diffuseAlbedo = MaterialConstantsCB.DiffuseAlbedo * Texture.Sample(LinearWrapSampler, pin.TexC);
    
    #ifdef ALPHA_TEST
        clip(diffuseAlbedo.a - 0.1f);
    #endif
    
    return;
}