struct VertexIn
{
    float3 position : POSITION;
    float3 color : COLOR;
};

struct VertexOut
{
    float4 color : COLOR;
    float4 position : SV_Position;
};

struct ObjectConstants
{
    matrix ModelMatrix;
};

ConstantBuffer<ObjectConstants> ObjectConstantsCB : register(b0);

struct PassConstants 
{
    float TotalTime;
    
    matrix View;
    matrix Proj;
    matrix ViewProj;
};

ConstantBuffer<PassConstants> PassConstantsCB :register(b1);

VertexOut main(VertexIn vin)
{
    VertexOut vout;
    
    float3 timeColorModulation = { 
        1 + sin(PassConstantsCB.TotalTime - 0.4),
        1 + cos(PassConstantsCB.TotalTime + 0.9),
        1 + sin(PassConstantsCB.TotalTime + 0.3)
    };
    
    timeColorModulation /= 2;
    
    vout.color = float4((vin.color * timeColorModulation),  1.0f);
    
    vout.position = mul(ObjectConstantsCB.ModelMatrix, float4(vin.position, 1.0f));
    vout.position = mul(PassConstantsCB.ViewProj, vout.position);
    
    return vout;
}