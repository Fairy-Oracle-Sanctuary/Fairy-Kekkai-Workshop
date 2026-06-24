#include "shared.hpp"
#include <vector>

// QueryVersion - reads file version info from a PE binary (DLL/EXE)
std::optional<version_t> QueryVersion(const std::wstring& exe)
{
    DWORD dwHandle;
    DWORD dwSize = GetFileVersionInfoSizeW(exe.c_str(), &dwHandle);
    if (dwSize == 0)
        return {};

    std::vector<char> versionInfoBuffer(dwSize);
    if (!GetFileVersionInfoW(exe.c_str(), dwHandle, dwSize, versionInfoBuffer.data()))
        return {};

    VS_FIXEDFILEINFO *pFileInfo;
    UINT fileInfoSize;
    if (!VerQueryValueW(versionInfoBuffer.data(), L"\\", reinterpret_cast<LPVOID *>(&pFileInfo), &fileInfoSize))
        return {};

    DWORD ms = pFileInfo->dwFileVersionMS;
    DWORD ls = pFileInfo->dwFileVersionLS;

    WORD majorVersion = HIWORD(ms);
    WORD minorVersion = LOWORD(ms);
    WORD buildNumber = HIWORD(ls);
    WORD revisionNumber = LOWORD(ls);
    return std::make_tuple(majorVersion, minorVersion, buildNumber, revisionNumber);
}

// SearchDllPath - stub: standalone build uses bundled onnxruntime, no system DLL searching
std::optional<std::wstring> SearchDllPath(const std::wstring &dll)
{
    return {};
}
