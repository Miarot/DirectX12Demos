struct PixelIn
{
    float3 norm : NORM;
};

struct MaterialConstants {
    float4 diffuseAlbedo;
    float3 fresnelR0;
    float roughness;
};

ConstantBuffer<MaterialConstants> MaterilaConstantsCB : register(b2);

float4 main(PixelIn pin) : SV_Target
{
    return float4((pin.norm + 1) / 2, 1.0f);
}