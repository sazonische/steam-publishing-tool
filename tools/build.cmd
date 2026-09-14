@echo off
setlocal
rem Usage: tools\build.cmd [debug|release|static]
rem   debug/release - dynamic Qt from %QT_ROOT% (fast iteration)
rem   static        - fully static exe, Qt built by vcpkg (slow the first time)
set MODE=%1
if "%MODE%"=="" set MODE=debug

if "%MODE%"=="debug" set PRESET=msvc-debug
if "%MODE%"=="release" set PRESET=msvc-release
if "%MODE%"=="static" set PRESET=static-release
if "%PRESET%"=="" (
	echo Unknown mode "%MODE%". Use debug, release or static.
	exit /b 1
)

if "%VS_ROOT%"=="" set VS_ROOT=C:\Program Files\Microsoft Visual Studio\18\Community
if "%QT_ROOT%"=="" set QT_ROOT=C:/Qt/6.8.3/msvc2022_64

rem vcvars64 overwrites VCPKG_ROOT with the vcpkg bundled into Visual Studio. Its scripts lag
rem behind the manifest baseline and break port builds, so we restore our own value afterwards.
set SPT_VCPKG_ROOT=%VCPKG_ROOT%
call "%VS_ROOT%\VC\Auxiliary\Build\vcvars64.bat" >nul
if "%SPT_VCPKG_ROOT%"=="" (set "VCPKG_ROOT=C:/vcpkg") else (set "VCPKG_ROOT=%SPT_VCPKG_ROOT%")
set PATH=%VS_ROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin;%VS_ROOT%\Common7\IDE\CommonExtensions\Microsoft\CMake\Ninja;%PATH%
echo Using VCPKG_ROOT=%VCPKG_ROOT%

cd /d "%~dp0.."
cmake --preset %PRESET% || exit /b 1
cmake --build --preset %PRESET% || exit /b 1
