Texture2D Frame : register(t0);

float4 main(float4 position : SV_Position) : SV_Target
{
    float4 gx = Frame.Load(int3(position.xy + int2(1, -1), 0)) - Frame.Load(int3(position.xy + int2(-1, -1), 0)) + 
                2 * Frame.Load(int3(position.xy + int2(1, 0), 0)) - 2 * Frame.Load(int3(position.xy + int2(-1, 0), 0)) +
                Frame.Load(int3(position.xy + int2(1, 1), 0)) - Frame.Load(int3(position.xy + int2(-1, 1), 0));
    
    float4 gy = Frame.Load(int3(position.xy + int2(-1, 1), 0)) - Frame.Load(int3(position.xy + int2(-1, -1), 0)) +
                2 * Frame.Load(int3(position.xy + int2(0, 1), 0)) - 2 * Frame.Load(int3(position.xy + int2(0, -1), 0)) +
                Frame.Load(int3(position.xy + int2(1, 1), 0)) - Frame.Load(int3(position.xy + int2(1, -1), 0));

     return sqrt(pow(gx, 2) + pow(gy, 2));
}