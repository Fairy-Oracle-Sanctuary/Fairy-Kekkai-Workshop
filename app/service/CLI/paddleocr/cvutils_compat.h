#pragma once

// Compatibility header replacing LunaTranslator's pch.h for the standalone paddleocr build.
// Force-included via CMake /FI option.

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <atlbase.h>
#include <atlcomcli.h>
#include <d3d11.h>
#include <dxgi.h>
#include <dxgi1_6.h>

#include <cassert>
#include <fstream>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <iostream>
#include <sstream>
#include <filesystem>
#include <cstdint>
#include <optional>
#include <functional>
#include <variant>
#include <algorithm>

using namespace std;

// Macros from LunaTranslator common2.hpp
#define CHECK_FAILURE_CONTINUE(x) \
    if (FAILED((x)))              \
        continue;

// For static build: no DLL export/import
#define DECLARE_API extern "C"
