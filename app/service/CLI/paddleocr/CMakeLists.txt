cmake_minimum_required(VERSION 3.20)
project(paddleocr LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Platform detection
# ---------------------------------------------------------------------------
if(${CMAKE_SIZEOF_VOID_P} EQUAL 8)
    set(platform "x64")
else()
    set(platform "x86")
endif()

# ---------------------------------------------------------------------------
# FetchContent dependencies
# ---------------------------------------------------------------------------
include(FetchContent)
set(FETCHCONTENT_UPDATES_DISCONNECTED ON)

# --- Clipper2 ---
FetchContent_Declare(clipper2
    GIT_REPOSITORY https://github.com/AngusJohnson/Clipper2
    GIT_TAG main
    GIT_SHALLOW TRUE
)

# --- OpenCV (prebuilt, same source as LunaTranslator) ---
FetchContent_Declare(opencv
    URL https://github.com/RapidAI/OpenCVBuilder/releases/download/4.7.0/opencv-4.7.0-windows-vs2019-md.7z
    DOWNLOAD_EXTRACT_TIMESTAMP true
)

FetchContent_MakeAvailable(opencv clipper2)

# ---------------------------------------------------------------------------
# Clipper2 static library
# ---------------------------------------------------------------------------
file(GLOB Clipper2LibSrc ${clipper2_SOURCE_DIR}/CPP/Clipper2Lib/src/*.cpp)
add_library(Clipper2Lib ${Clipper2LibSrc})
target_include_directories(Clipper2Lib PUBLIC ${clipper2_SOURCE_DIR}/CPP/Clipper2Lib/include)

# ---------------------------------------------------------------------------
# OpenCV (interface library)
# ---------------------------------------------------------------------------
add_library(libOpenCV INTERFACE)
target_include_directories(libOpenCV INTERFACE "${opencv_SOURCE_DIR}/windows-${platform}/include")
file(GLOB OpenCV_LIBS "${opencv_SOURCE_DIR}/windows-${platform}/${platform}/vc16/staticlib/*.lib")
target_link_libraries(libOpenCV INTERFACE "${OpenCV_LIBS}")

# ---------------------------------------------------------------------------
# ONNX Runtime (via LunaTranslator's libonnxruntime.cmake)
# ---------------------------------------------------------------------------
set(WIN10ABOVE ON)
set(CMAKE_FINAL_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
include(libonnxruntime.cmake)

# ---------------------------------------------------------------------------
# CVUtils source files compiled as a static library
# ---------------------------------------------------------------------------
add_library(CVUtilsStatic STATIC
    dbcrnn.cpp
    ortwrapper.cpp
    localocr.cpp
    similarity.cpp
    shared.cpp
)

target_include_directories(CVUtilsStatic PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# Force-include compatibility header as PCH replacement
target_compile_options(CVUtilsStatic PRIVATE /FI"${CMAKE_CURRENT_SOURCE_DIR}/cvutils_compat.h" /utf-8 /EHa)

target_compile_definitions(CVUtilsStatic PRIVATE
    UNICODE
    _UNICODE
    WIN10ABOVE=1
)

target_link_libraries(CVUtilsStatic PRIVATE
    Clipper2Lib
    libOpenCV
    onnxruntime
    dxgi
    d3d12
    version
    delayimp
)

# CVUtils uses delay-loaded D3D12/DXGI
target_link_options(CVUtilsStatic PRIVATE
    "/DELAYLOAD:d3d12.dll"
    "/DELAYLOAD:dxgi.dll"
)

# ---------------------------------------------------------------------------
# paddleocr executable
# ---------------------------------------------------------------------------
add_executable(paddleocr main.cpp)

target_include_directories(paddleocr PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_options(paddleocr PRIVATE /utf-8)

target_compile_definitions(paddleocr PRIVATE
    UNICODE
    _UNICODE
)

target_link_libraries(paddleocr PRIVATE
    CVUtilsStatic
    libOpenCV
    onnxruntime
    ws2_32
    user32
    gdi32
    winmm
    imm32
    ole32
    oleaut32
    comdlg32
    advapi32
    shell32
)

# Copy runtime DLLs next to paddleocr.exe after build
add_custom_command(TARGET paddleocr POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${onnxruntime_DLL}"
        "$<TARGET_FILE_DIR:paddleocr>/onnxruntime.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${directml_DLL}"
        "$<TARGET_FILE_DIR:paddleocr>/DirectML.dll"
)
