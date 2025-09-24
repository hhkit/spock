In this repository, we try to follow [vkguide.dev](https://vkguide.dev) with the following changes:
- Using Slang/Slangc instead of shaderc.
- Developing on a Mac.


# Building

```bash
git clone https://github.com/hhkit/spock
cd spock
cmake --B build .
cmake --build build
```

You'll need the VulkanSDK installed and findable in your environment.