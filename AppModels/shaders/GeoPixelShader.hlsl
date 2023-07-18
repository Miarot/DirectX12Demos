#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 0
#endif

#include "Common.hlsli"
#include "GeoUtils.hlsli"

ConstantBuffer<PassConstants> PassConstantsCB : register(b1);
ConstantBuffer<MaterialConstants> MaterilaConstantsCB : register(b2);

Texture2D Texture : register(t0);
Texture2D OcclusionMap : register(t1);
Texture2D ShadowMap[NUM_DIR_LIGHTS + NUM_POINT_LIGHTS] : register(t2);

SamplerState LinearWrapSampler : register(s0);
SamplerComparisonState PointClumpSampler : register(s1);

float CalcShadowFactor(float3 posW, int index) {
    float4 projTexC = mul(PassConstantsCB.Lights[index].LightViewProjTex, float4(posW, 1.0f));
    projTexC = projTexC / projTexC.w;
    
    float depthCur = projTexC.z;

    uint width, height, mipMaps;
    ShadowMap[index].GetDimensions(0, width, height, mipMaps);
    
    float dx = 1 / (float) width;
    float dy = 1 / (float) height;
    
    const int numSamples = 4;
    const float2 offsets[] = {
        float2(0.0f, 0.0f), float2(dx, 0.0f),
        float2(0.0f, dy), float2(dx, dy)
    };
    
    float shadowFactor = 0.0f;
    
    for (int i = 0; i < numSamples; ++i) {
        shadowFactor += ShadowMap[index].SampleCmpLevelZero(PointClumpSampler, projTexC.xy + offsets[i], depthCur).r;
    }
    
    shadowFactor /= (float)numSamples;
    
    return shadowFactor;
}

float4 main(VertexOut pin) : SV_Target
{
    // init material from MaterialConstants and texture
    Material mat = {
        MaterilaConstantsCB.DiffuseAlbedo * Texture.Sample(LinearWrapSampler, pin.TexC),
        MaterilaConstantsCB.FresnelR0,
        1 - MaterilaConstantsCB.Roughness
    };
    
    // discard pixel if it is transparent
    #ifdef ALPHA_TEST
        clip(mat.DiffuseAlbedo.a - 0.1f);
    #endif
    
    // normals may lose unit leght during interpolation
    float3 norm = normalize(pin.Norm);
    
    // for normals view and ssao normals render
    #ifdef DRAW_NORMS
        #ifdef SSAO
            // for ssao need normals in view space
            float3 normV = mul((float3x3)PassConstantsCB.View, norm);
            return float4(normV, 0.0f);
        #endif
    
        return float4((norm + 1) / 2, 1.0f);
    #endif
    
    // compute inderect light
    float4 inderectLight = mat.DiffuseAlbedo * PassConstantsCB.AmbientLight;
    
    // sample occlusion map and modulate inderect light
    #ifdef SSAO
        float4 occlusionMapTexC = mul(PassConstantsCB.ViewProjTex, float4(pin.PosW, 1.0f));
        occlusionMapTexC /= occlusionMapTexC.w;
        float occlusion = OcclusionMap.SampleLevel(LinearWrapSampler, occlusionMapTexC.xy, 0.0f).r;
        
        #ifdef SSAO_ONLY
            return occlusion;
        #endif
    
        inderectLight *= occlusion;
    #endif
    
    // init shadow factors
    float shadowFactors[MaxLights];
    
    for (int i = 0; i < MaxLights; ++i) {
        shadowFactors[i] = 1.0f;
    }
    
    // compute shadow factors for directional and spot lights
    for (i = 0; i < NUM_DIR_LIGHTS + NUM_SPOT_LIGHTS; ++i) {
        shadowFactors[i] = CalcShadowFactor(pin.PosW, i);
    }

    // compute direct light
    float3 toEye = normalize(PassConstantsCB.EyePos - pin.PosW);
    float4 directLight = ComputeLighting(PassConstantsCB.Lights, mat, norm, toEye, pin.PosW, shadowFactors);
    
    // compute final color
    float4 color = inderectLight + directLight;
        
    // It is said that it is common convention to take alpha from diffuse material
    color.a = mat.DiffuseAlbedo.a;
    
    return color;
}