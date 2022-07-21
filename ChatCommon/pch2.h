// pch.h: This is a precompiled header file.
// Files listed below are compiled only once, improving build performance for future builds.
// This also affects IntelliSense performance, including code completion and many code browsing features.
// However, files listed here are ALL re-compiled if any one of them is updated between builds.
// Do not add files here that you will be updating frequently as this negates the performance advantage.

#ifndef PCH_H2
#define PCH_H2

// add headers that you want to pre-compile here
#include "framework.h"
#include <crtdbg.h>
#include <vector>
#include <unordered_set>
#include <list>
#include <map>
#include <string>
#include <thread>
#include <mutex>
#include <chrono>
#include <filesystem>

#include <crtdefs.h>
#include <process.h>
#include <WinSock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")

#include "imgui/imgui.h"
#include "imgui/imgui_impl_dx11.h"
#include "imgui/imgui_impl_win32.h"
//#include "imgui/imgui_stdlib.h"

#include <wrl/client.h>
using namespace Microsoft::WRL;

// DirectX 11 Header
#include <d3d11.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>

using namespace DirectX;
using namespace DirectX::PackedVector;

// DirectX Library
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "dxguid")

#include "struct.h"

#endif //PCH_H
