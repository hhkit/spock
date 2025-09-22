// tutorial.slang
[format("rgba16f")] WTexture2D<float4> image;

[shader("compute")]
[numthreads(16,16,1)]
void computeMain(uint3 threadId : SV_DispatchThreadID
  , uint3 localThreadId : SV_GroupThreadID
  )
{
  int2 texelCoord = threadId.xy;
  int2 size;
  image.GetDimensions(size.x, size.y);

  if(texelCoord.x < size.x && texelCoord.y < size.y)
  {
      float4 color = float4(0.0h, 0.0h, 0.0h, 1.0h);

      if(localThreadId.x != 0 && localThreadId.y != 0)
      {
          color.x = float(texelCoord.x)/size.x;
          color.y = float(texelCoord.y)/size.y;
      }
      

      image.Store(texelCoord, color);
  }
}
