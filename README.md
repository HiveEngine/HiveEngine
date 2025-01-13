# Hive
Hive Engine – Multi-platform 2D and 3D game engine

## Building from source
`git clone https://github.com/HiveEngine/HiveEngine.git`

`cd HiveEngine`

`git submodule update --init`


Please make sure you have a valid vulkan sdk installed on your device at a known location

### With Visual studio
We use cmake as our build system, you will need to create the solution file for the project. 

1. `cmake -G "Visual Studio 17 2022" -DVULKAN_SDK_PATH=C:\VulkanSDK\1.3.296.0`
2. Open the solution in visual studio and set the _Sandbox_ project as the startup project
3. Run

** If the application crash, make sure that the working directory is set to the Sandbox folder

### With Clion

1. Open the directory in clion and 
2. In the cmake option of clion add the following line: `-DVULKAN_SDK_PATH=C:\Libraries\VulkanSDK\1.3.296.0`
3. In the Sandbox run configuration, set the working directory to be the Sandbox folder
4. Run

### Linux
Usually the package manager of your distro will put the vulkan sdk at the default path, so you don't need to provide the vulkan sdk path in this case
1. `mkdir build && cd build`
2. `cmake ..` or `cmake -DVULKAN_SDK_PATH=/non_standard_sdk_path ..`
3. `cmake --build .`
4. `cd ../Sandbox`
5. `../build/Sandbox/Sandbox`