// tutorial.slang
[format("rgba16f")] WTexture2D<float4> image;

// struct constants {
//   float4 data1;
//   float4 data2;
//   float4 data3;
//   float4 data4;
// }

[shader("compute")]
[numthreads(16,16,1)]
void computeMain(uint3 threadId : SV_DispatchThreadID
  //, [vk::push_constant] uniform constants PushConstants
  )
{
  int2 texelCoord = threadId.xy;
  int2 size;
  image.GetDimensions(size.x, size.y);

  // var topColor = PushConstants.data1;
  // var bottomColor = PushConstants.data2;

  if(texelCoord.x < size.x && texelCoord.y < size.y)
  {
      float4 color = float4(0.0h, 0.0h, 0.0h, 1.0h);

      if(threadId.x != 0 && threadId.y != 0)
      {
          color.x = float(texelCoord.x)/(size.x);
          color.y = float(texelCoord.y)/(size.y);
      }
      
      // var blend = float(texelCoord.y) / size.y;
      // var color = lerp(topColor, bottomColor, blend);

      image.Store(texelCoord, color);
  }
}
