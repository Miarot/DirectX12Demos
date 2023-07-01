#include <GeoConstantsStructures.hlsl>

struct PixelIn
{
    float3 norm : NORM;
};

ConstantBuffer<PassConstants> PassConstantsCB : register(b1);
ConstantBuffer<MaterialConstants> MaterilaConstantsCB : register(b2);

float4 main(PixelIn pin) : SV_Target
{
    float4 color;
    
    if (PassConstantsCB.isDrawNorm == 1) 
    {
        color = float4((pin.norm + 1) / 2, 1.0f);
    } else {
        color = float4(MaterilaConstantsCB.fresnelR0, 1.0f);
    }
    
    return color;
}