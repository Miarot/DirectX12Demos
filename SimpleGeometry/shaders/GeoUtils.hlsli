#include "LightUtils.hlsli"

struct VertexIn
{
    float3 Pos : POSITION;
    float3 Norm : NORM;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosW : POSITION;
    float3 Norm : NORM;
    float2 TexC : TEXCOORD;
};

struct ObjectConstants
{
    matrix ModelMatrix;
};

struct PassConstants
{
    matrix View;
    matrix Proj;
    matrix ViewProj;
    
    float3 EyePos;
    float TotalTime;
    
    float4 AmbientLight;
    // first NUM_DIR_LIGHTS --- directional lights
    // next NUM_POINT_LIGHTS --- point lights
    // last NUM_SPOT_LIGHTS --- spot lights
    Light Lights[MaxLights]; 
};

struct MaterialConstants
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
};
