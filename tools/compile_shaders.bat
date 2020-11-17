@echo off
set SHADERS_SRC_DIR=..\shaders
set SHADERS_BUILD_DIR=..\build\shaders
if not exist %SHADERS_BUILD_DIR% mkdir %SHADERS_BUILD_DIR%
set GLSLC=%VK_SDK_PATH%\Bin\glslc.exe
echo Compiling shaders...

REM Vert shaders
for /r %SHADERS_SRC_DIR% %%f in (*.vert.glsl) do (
	echo "%SHADERS_SRC_DIR%\%%~nxf -> %SHADERS_BUILD_DIR%\%%~nf.spv"
	call %GLSLC% -fshader-stage=vert %SHADERS_SRC_DIR%\%%~nxf -o %SHADERS_BUILD_DIR%\%%~nf.spv
)

REM Frag shaders
for /r %SHADERS_SRC_DIR% %%f in (*.frag.glsl) do (
	echo "%SHADERS_SRC_DIR%\%%~nxf -> %SHADERS_BUILD_DIR%\%%~nf.spv"
	call %GLSLC% -fshader-stage=frag %SHADERS_SRC_DIR%\%%~nxf -o %SHADERS_BUILD_DIR%\%%~nf.spv
)