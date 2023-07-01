struct VertexIn
{
    float3 position : POSITION;
    float3 norm : NORM;
};

struct VertexOut
{
    float3 norm : NORM;
    float4 position : SV_Position;
};

struct ObjectConstants
{
    matrix ModelMatrix;
};

ConstantBuffer<ObjectConstants> ObjectConstantsCB : register(b0);

struct PassConstants 
{ 
    matrix View;
    matrix Proj;
    matrix ViewProj;
    
    float TotalTime;
    uint isDrawNorm;
};

ConstantBuffer<PassConstants> PassConstantsCB :register(b1);

VertexOut main(VertexIn vin)
{
    VertexOut vout;
  
    vout.norm = vin.norm;
    
    vout.position = mul(ObjectConstantsCB.ModelMatrix, float4(vin.position, 1.0f));
    vout.position = mul(PassConstantsCB.ViewProj, vout.position);
    
    return vout;
}