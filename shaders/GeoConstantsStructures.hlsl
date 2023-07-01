struct ObjectConstants
{
    matrix ModelMatrix;
};

struct PassConstants
{
    matrix View;
    matrix Proj;
    matrix ViewProj;
    
    float TotalTime;
    uint isDrawNorm;
};

struct MaterialConstants
{
    float4 diffuseAlbedo;
    float3 fresnelR0;
    float roughness;
};
