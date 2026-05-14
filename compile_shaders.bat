@echo off
REM Shader compilation script for GreatWar GPU (Windows)
REM Usage: compile_shaders.bat [output_directory]

setlocal enabledelayedexpansion

if "%1"=="" (
    set OUTPUT_DIR=.
) else (
    set OUTPUT_DIR=%1
)

set SHADER_DIR=%~dp0shaders

if not exist "%SHADER_DIR%" (
    echo Error: Shader directory not found at %SHADER_DIR%
    exit /b 1
)

REM Check if glslc is available
where glslc >nul 2>nul
if %ERRORLEVEL% NEQ 0 (
    echo Error: glslc not found. Please install Vulkan SDK and add it to PATH.
    exit /b 1
)

echo Compiling shaders to SPIR-V...
echo ================================

REM Create output directory if it doesn't exist
if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

set SUCCESS=0
set FAILED=0

REM Compile each shader
for %%s in (crowd.comp crowd.vert crowd.frag) do (
    set INPUT=%SHADER_DIR%\%%s
    set OUTPUT=%OUTPUT_DIR%\%%s.spv
    
    if exist "!INPUT!" (
        echo Compiling %%s...
        glslc "!INPUT!" -o "!OUTPUT!"
        if !ERRORLEVEL! EQU 0 (
            echo   OK: !OUTPUT!
            set /a SUCCESS+=1
        ) else (
            echo   FAILED: %%s
            set /a FAILED+=1
        )
    ) else (
        echo   NOT FOUND: !INPUT!
        set /a FAILED+=1
    )
)

echo ================================
echo Compilation summary:
echo   Successful: %SUCCESS%
echo   Failed: %FAILED%

if %FAILED% EQU 0 (
    echo All shaders compiled successfully!
    exit /b 0
) else (
    echo Some shaders failed to compile.
    exit /b 1
)
