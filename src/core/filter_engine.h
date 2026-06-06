#pragma once
#include <vector>
#include <string>
#include <fstream>
#include <cwctype>
#include "win32_utils.h"

// Glob tabanlı joker karakter eşleştirme fonksiyonu (* ve ? destekler)
inline bool MatchWildcard(const wchar_t* pattern, const wchar_t* str) {
    if (*pattern == L'\0' && *str == L'\0') return true;
    if (*pattern == L'*') {
        while (*(pattern + 1) == L'*') pattern++;
        if (*(pattern + 1) == L'\0') return true;
        while (*str != L'\0') {
            if (MatchWildcard(pattern + 1, str)) return true;
            str++;
        }
        return false;
    }
    if (*pattern == L'?' || towlower(*pattern) == towlower(*str)) {
        if (*str == L'\0') return false;
        return MatchWildcard(pattern + 1, str + 1);
    }
    return false;
}

class FilterEngine {
public:
    FilterEngine() {
        LoadRules();
    }

    void LoadRules() {
        m_rules.clear();
        std::ifstream file("rules.txt");
        if (!file.is_open()) return;
        std::string line;
        while (std::getline(file, line)) {
            if (line.empty()) continue;
            m_rules.push_back(ANSIToWString(line));
        }
    }

    void SaveRules() {
        std::ofstream file("rules.txt");
        if (!file.is_open()) return;
        for (const auto& rule : m_rules) {
            file << WStringToANSI(rule) << "\n";
        }
    }

    void SetRules(const std::vector<std::wstring>& rules) {
        m_rules = rules;
        SaveRules();
    }

    std::vector<std::wstring> GetRules() const {
        return m_rules;
    }

    bool ShouldBackup(const std::wstring& filePath) const {
        if (m_rules.empty()) return true;

        size_t pos = filePath.find_last_of(L"\\/");
        std::wstring filename = (pos == std::wstring::npos) ? filePath : filePath.substr(pos + 1);

        bool hasIncludeRules = false;
        bool matchedInclude = false;
        bool matchedExclude = false;

        for (const auto& rule : m_rules) {
            if (rule.length() < 2) continue;
            wchar_t action = rule[0];
            std::wstring pattern = rule.substr(1);

            // Hem dosya adına hem de tam yola karşı glob eşleme yaparız
            bool match = MatchWildcard(pattern.c_str(), filename.c_str()) || 
                         MatchWildcard(pattern.c_str(), filePath.c_str());

            if (action == L'+') {
                hasIncludeRules = true;
                if (match) matchedInclude = true;
            } else if (action == L'-') {
                if (match) matchedExclude = true;
            }
        }

        if (matchedExclude) return false;
        if (hasIncludeRules) return matchedInclude;
        return true;
    }

private:
    std::vector<std::wstring> m_rules;
};
