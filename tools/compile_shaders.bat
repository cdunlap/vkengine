if not exist "build\shaders" mkdir "build\shaders"
GLSLC=%VK_SDK_PATH%\Bin\glslc.exe
echo "Compiling shaders..."
echo "shaders/main.vert.glsl -> build/shaders/main.vert.spv"
%GLSLC% -fshader-stage=vert shaders/main.vert.glsl -o build/shaders/main.vert.spv
