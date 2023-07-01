struct PixelIn
{
    float4 color : COLOR;
};

struct MaterialConstants {
    float4 diffuseAlbedo;
    float3 fresnelR0;
    float roughness;
};

ConstantBuffer<MaterialConstants> MaterilaConstantsCB : register(b2);

float4 main(PixelIn pin) : SV_Target
{
    return float4(MaterilaConstantsCB.fresnelR0, 1.0f);
}