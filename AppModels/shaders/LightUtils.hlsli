#define MaxLights 16

#ifndef NUM_DIR_LIGHTS
    #define NUM_DIR_LIGHTS 1
#endif

#ifndef NUM_POINT_LIGHTS
    #define NUM_POINT_LIGHTS 1
#endif

#ifndef NUM_SPOT_LIGHTS
    #define NUM_SPOT_LIGHTS 1
#endif

struct Material
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Shininess;
};

struct Light
{
    float3 Strength;
    float FalloffStart;
    float3 Direction;
    float FalloffEnd;
    float3 Position;
    float SpotPower;
        
    matrix LightViewProj;
    matrix LightViewProjTex;
};

float CalcAttenuation(float falloffStart, float falloffEnd, float d) {
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

float3 SchlickFresnel(float3 R0, float3 norm, float3 lightVec) {
    float cosTheta = saturate(dot(norm, lightVec));
    return R0 + (1.0f - R0) * pow((1 - cosTheta), 5);
}

float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 norm, float3 toEye, Material mat) {
    float m = mat.Shininess * 256.0f;
    float3 h = normalize(toEye + lightVec);
    
    // approximate Fresnel factor with Schlick formula
    float3 SpecularAlbedo = SchlickFresnel(mat.FresnelR0, norm, lightVec);
    // model roughness
    SpecularAlbedo *= (8.0f + m) * pow(max(dot(norm, h), 0.0f), m) / 8.0f;
    // scale down to range from 0 to 1
    SpecularAlbedo = SpecularAlbedo / (SpecularAlbedo + 1.0f);
    
    return lightStrength * (mat.DiffuseAlbedo.rgb + SpecularAlbedo);
}

float3 Phong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    float m = mat.Shininess * 256.0f;
    float3 r = reflect(-lightVec, normal);
    
    // model roughness
    float SpecularAlbedo = pow(max(dot(toEye, r), 0.0f), m);
    
    return lightStrength * (mat.DiffuseAlbedo.rgb + SpecularAlbedo);
}

float3 ModulateLight(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat) {
    #ifdef PHONG
        return Phong(lightStrength, lightVec, normal, toEye, mat);
    #else
        return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
    #endif
}


float3 ComputeDirectionalLight(Light L, Material mat, float3 norm, float3 toEye) {
    float3 lightVec = -L.Direction;
    // Lambert's cosine law
    float3 lightStrength = L.Strength * max(dot(norm, lightVec), 0);
    
    return ModulateLight(lightStrength, lightVec, norm, toEye, mat);
}

float3 ComputePointLight(Light L, Material mat, float3 norm, float3 toEye, float3 pos) {
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);
    
    if (d > L.FalloffEnd) {
        return 0.0f;
    }
    
    lightVec /= d;
    
    // linear attenuation with distance
    float3 lightStrength = L.Strength * CalcAttenuation(L.FalloffStart, L.FalloffEnd, d);
    // Lambert's cosine law
    lightStrength *= max(dot(norm, lightVec), 0);
    
    return ModulateLight(lightStrength, lightVec, norm, toEye, mat);
}

float3 ComputeSpotLight(Light L, Material mat, float3 norm, float3 toEye, float3 pos) {
    float3 lightVec = L.Position - pos;
    float d = length(lightVec);
    
    if (d > L.FalloffEnd) {
        return 0.0f;
    }
    
    lightVec /= d;
    
    // spot light attenuation with angle between light's direction and light vector
    float3 lightStrength = L.Strength * pow(max(dot(L.Direction, -lightVec), 0), L.SpotPower);
    // linear attenuation with distance
    lightStrength *= CalcAttenuation(L.FalloffStart, L.FalloffEnd, d);
    // Lambert's cosine law
    lightStrength *= max(dot(norm, lightVec), 0);
    
    return ModulateLight(lightStrength, lightVec, norm, toEye, mat);
}

float4 ComputeLighting(Light lights[MaxLights], Material mat, float3 norm, float3 toEye, float3 pos, float shadowFactors[MaxLights]) {
    float3 res = 0.0f;
    int i = 0;
    
#if (NUM_DIR_LIGHTS > 0)
        [unroll]
        for (i = 0; i < NUM_DIR_LIGHTS; ++i) {
            res += shadowFactors[i] * ComputeDirectionalLight(lights[i], mat, norm, toEye);
        }
    #endif

    #if (NUM_SPOT_LIGHTS > 0)
        //[unroll]
        for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_SPOT_LIGHTS; ++i) {
            res += shadowFactors[i] * ComputeSpotLight(lights[i], mat, norm, toEye, pos);
        }
    #endif
    
    #if (NUM_POINT_LIGHTS > 0)
        [unroll]
        for (i = NUM_DIR_LIGHTS + NUM_SPOT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_SPOT_LIGHTS + NUM_POINT_LIGHTS; ++i) {
            res += shadowFactors[i] * ComputePointLight(lights[i], mat, norm, toEye, pos);
        }
    #endif
    
    
    return float4(res, 0.0f);
}