#pragma once
#include <cstdint>

// CVUtils C API declarations (standalone build - no dllimport)
// The CVUtils source is compiled directly into paddleocr.exe

namespace cv { class Mat; }

enum class Directional
{
    H,
    V,
    Auto
};

extern "C" {
    bool OcrLoadRuntime();
    void* OcrInit(
        const wchar_t* szDetModel,
        const wchar_t* szRecModel,
        const wchar_t* szKeyPath,
        int nThreads,
        bool gpu,
        uint64_t luid,
        const char* device_type,
        void (*cb2)(const char*)
    );
    void OcrDetect(
        void* pOcrObj,
        const cv::Mat* mat,
        Directional mode,
        void (*cb)(float, float, float, float, float, float, float, float, float, const char*),
        void (*cb2)(const char*)
    );
    void OcrDestroy(void* pOcrObj);
}
