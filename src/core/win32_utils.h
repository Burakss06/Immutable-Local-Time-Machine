#pragma once
#include <windows.h>
#include <string>

// wstring yolunu sistem varsayılan ANSI kod sayfasına (CP_ACP) çevirir.
// Bu sayede MinGW'nin wide-stream kısıtlamalarını aşarız.
inline std::string WStringToANSI(const std::wstring& wstr) {
    if (wstr.empty()) return "";
    int size_needed = WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (size_needed <= 0) return "";
    std::string strTo(size_needed - 1, 0);
    WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1, &strTo[0], size_needed, nullptr, nullptr);
    return strTo;
}

// ANSI stringi wstring (Unicode) formatına çevirir.
inline std::wstring ANSIToWString(const std::string& str) {
    if (str.empty()) return L"";
    int size_needed = MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, nullptr, 0);
    if (size_needed <= 0) return L"";
    std::wstring wstrTo(size_needed - 1, 0);
    MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, &wstrTo[0], size_needed);
    return wstrTo;
}

// Dosyanın var olup olmadığını Win32 API ile kontrol eder
inline bool Win32FileExists(const std::wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && !(dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

// Klasörün var olup olmadığını Win32 API ile kontrol eder
inline bool Win32DirectoryExists(const std::wstring& path) {
    DWORD dwAttrib = GetFileAttributesW(path.c_str());
    return (dwAttrib != INVALID_FILE_ATTRIBUTES && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY));
}

// Dosya boyutunu Win32 API ile çeker
inline uint64_t Win32GetFileSize(const std::wstring& path) {
    WIN32_FILE_ATTRIBUTE_DATA fad;
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)) {
        return 0;
    }
    LARGE_INTEGER size;
    size.LowPart = fad.nFileSizeLow;
    size.HighPart = fad.nFileSizeHigh;
    return static_cast<uint64_t>(size.QuadPart);
}
