#include "LightUtils.hlsli"

struct ObjectConstants
{
    matrix ModelMatrix;
    matrix ModelMatrixInvTrans;
};

struct PassConstants
{
    matrix View;
    matrix Proj;
    matrix ViewProj;
    matrix ProjInv;
    matrix ProjTex;
    matrix ViewProjTex;
    
    float3 EyePos;
    float TotalTime;
    
    float4 AmbientLight;
    // first NUM_DIR_LIGHTS --- directional lights
    // next NUM_SPOT_LIGHTS --- spot lights
    // last NUM_POINT_LIGHTS --- point lights
    Light Lights[MaxLights];
    
    float4 RandomDirections[14];
    float OcclusionRadius;
    float OcclusionFadeStart;
    float OcclusionFadeEnd;
    float OcclusionEpsilon;
    
    float OcclusionMapWidthInv;
    float OcclusionMapHeightInv;
};

struct MaterialConstants
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
};
