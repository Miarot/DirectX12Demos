struct PixelIn
{
    float3 norm : NORM;
};

struct PassConstants
{
    matrix View;
    matrix Proj;
    matrix ViewProj;
    
    float TotalTime;
    uint isDrawNorm;
};

ConstantBuffer<PassConstants> PassConstantsCB : register(b1);

struct MaterialConstants {
    float4 diffuseAlbedo;
    float3 fresnelR0;
    float roughness;
};

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