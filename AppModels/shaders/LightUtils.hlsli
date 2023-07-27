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
    float Metallic;
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

static const float PI = 3.14159265359f;

float CalcAttenuation(float falloffStart, float falloffEnd, float d) {
    return saturate((falloffEnd - d) / (falloffEnd - falloffStart));
}

// Schlick's approximation for Fresnel factor
float3 SchlickFresnel(float3 R0, float3 normal, float3 lightVec) {
    float cosTheta = saturate(dot(normal, lightVec));
    return R0 + (1.0f - R0) * pow((1 - cosTheta), 5.0f);
}

// Trowbridge-Reitz GGX normal distribution function
float DistributionGGX(float3 normal, float3 h, float roughness) {
    float a2 = roughness * roughness;
    float NormalDotH2 = max(dot(normal, h), 0.0f);
    NormalDotH2 = NormalDotH2 * NormalDotH2;
    
    float num = a2;
    float denom = NormalDotH2 * (a2 - 1.0f) + 1.0f;
    denom = PI * denom * denom;
    
    return num / denom;
}

// Schlick-GGX geometry attenuation factor
float GeometrySchlickGGX(float3 normal, float3 V, float roughness) {
    float k = roughness + 1.0f;
    k = k * k / 8.0f;
    float NormalDotV = max(dot(normal, V), 0.0f);
    
    float num = NormalDotV;
    float denom = NormalDotV * (1.0f - k) + k;
    
    return num / denom;
}

// Smith's method to consider both geometry obstruction and geometry shadowing
float GeometrySmith(float3 normal, float3 toEye, float3 lightVec, float roughness) {
    // geometry obstruction
    float ggx1 = GeometrySchlickGGX(normal, toEye, roughness);
    // geometry shadowing
    float ggx2 = GeometrySchlickGGX(normal, lightVec, roughness);
    
    return ggx1 * ggx2;
}

// from "Introduction to 3D Game Programming with DirectX 12" Frank D. Luna
// Blinn-Phong model with Schlick's Fresnel factor approximation and some energy conservation
float3 BlinnPhongModified(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    float m = clamp(mat.Shininess * 256.0f, 1.0f, 256.0f);
    float3 h = normalize(toEye + lightVec);
    
    // approximate Fresnel factor with Schlick formula
    float3 SpecularAlbedo = SchlickFresnel(mat.FresnelR0, normal, lightVec);
    // model roughness
    SpecularAlbedo *= (8.0f + m) * pow(max(dot(normal, h), 0.0f), m) / 8.0f;
    // scale down to range from 0 to 1
    SpecularAlbedo = SpecularAlbedo / (SpecularAlbedo + 1.0f);
    
    return lightStrength * (mat.DiffuseAlbedo.rgb + SpecularAlbedo);
}

float3 Phong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    float m = clamp(mat.Shininess * 64.0f, 1.0f, 64.0f);
    float3 r = reflect(-lightVec, normal);
    
    // model roughness
    float SpecularAlbedo = pow(max(dot(toEye, r), 0.0f), m);
    
    return lightStrength * (mat.DiffuseAlbedo.rgb + 0.3 * SpecularAlbedo);
}

float3 BlinnPhong(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat)
{
    float m = clamp(mat.Shininess * 256.0f, 1.0f, 256.0f);
    float3 h = normalize(toEye + lightVec);
    
    // model roughness
    float SpecularAlbedo = pow(max(dot(normal, h), 0.0f), m);
    
    return lightStrength * (mat.DiffuseAlbedo.rgb + 0.3 * SpecularAlbedo);
}

// PBR from https://learnopengl.com/PBR/Theory
float3 DisneyPBR(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat) {
    float3 h = normalize(toEye + lightVec);
    float roughness = 1.0f - mat.Shininess;
    
    // set F0 depending on metallic
    mat.FresnelR0 = lerp(mat.FresnelR0, mat.DiffuseAlbedo.xyz, mat.Metallic);
    // calculate Fresnel factor
    float3 F = SchlickFresnel(mat.FresnelR0, normal, lightVec);
    // calculate normal distribution function
    float D = DistributionGGX(normal, h, roughness);
    // calculate geometry attenuation factor
    float G = GeometrySmith(normal, toEye, lightVec, roughness);
    
    // calculate Cook-Torrance BRDF
    float3 numerator = D * G * F;
    float denominator = 4.0f * max(dot(normal, toEye), 0.0f) * max(dot(normal, lightVec), 0.0f) + 0.0001f;
    float3 specular = numerator / denominator;
    float m = clamp(mat.Shininess * 256.0f, 1.0f, 256.0f);
    
    // specular light fraction
    float3 kS = F;
    // diffuse light fraction
    float3 kD = 1.0f - kS;
    // if material is metallic there is no diffuse reflection
    kD *= 1.0f - mat.Metallic;
    
    return (kD * mat.DiffuseAlbedo.xyz / PI + specular) * lightStrength;
}

float3 ComputeShading(float3 lightStrength, float3 lightVec, float3 normal, float3 toEye, Material mat) {
    #ifdef PHONG
        return Phong(lightStrength, lightVec, normal, toEye, mat);
    #elif defined BLINNPHONG
        return BlinnPhong(lightStrength, lightVec, normal, toEye, mat);
    #elif defined PBR
        return DisneyPBR(lightStrength, lightVec, normal, toEye, mat);
    #else
        return BlinnPhongModified(lightStrength, lightVec, normal, toEye, mat);
    #endif
}


float3 ComputeDirectionalLight(Light L, Material mat, float3 norm, float3 toEye) {
    float3 lightVec = -L.Direction;
    // Lambert's cosine law
    float3 lightStrength = L.Strength * max(dot(norm, lightVec), 0);
    
    return ComputeShading(lightStrength, lightVec, norm, toEye, mat);
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
    
    return ComputeShading(lightStrength, lightVec, norm, toEye, mat);
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
    
    return ComputeShading(lightStrength, lightVec, norm, toEye, mat);
}

float4 ComputeLighting(Light lights[MaxLights], Material mat, float3 normal, float3 toEye, float3 pos, float shadowFactors[MaxLights]) {
    float3 res = 0.0f;
    int i = 0;
    
#if (NUM_DIR_LIGHTS > 0)
        [unroll]
        for (i = 0; i < NUM_DIR_LIGHTS; ++i) {
            res += shadowFactors[i] * ComputeDirectionalLight(lights[i], mat, normal, toEye);
        }
    #endif

    #if (NUM_SPOT_LIGHTS > 0)
        //[unroll]
        for (i = NUM_DIR_LIGHTS; i < NUM_DIR_LIGHTS + NUM_SPOT_LIGHTS; ++i) {
            res += shadowFactors[i] * ComputeSpotLight(lights[i], mat, normal, toEye, pos);
        }
    #endif
    
    #if (NUM_POINT_LIGHTS > 0)
        [unroll]
        for (i = NUM_DIR_LIGHTS + NUM_SPOT_LIGHTS; i < NUM_DIR_LIGHTS + NUM_SPOT_LIGHTS + NUM_POINT_LIGHTS; ++i) {
            res += shadowFactors[i] * ComputePointLight(lights[i], mat, normal, toEye, pos);
        }
    #endif
    
    
    return float4(res, 0.0f);
}