#include "console_ui.h"
#include <conio.h>

// Konsol giriş kuyruğundan basılan tuşu okur
int ConsoleUI::ReadMenuInput() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE) return 0;

    // Konsolun ham giriş modunu (mouse ve pencere boyutlandırma olaylarını kapatarak) ayarlayabiliriz
    DWORD prevMode;
    GetConsoleMode(hIn, &prevMode);
    SetConsoleMode(hIn, ENABLE_EXTENDED_FLAGS); // Fare ve pencere olaylarını süz

    INPUT_RECORD record;
    DWORD read = 0;

    while (true) {
        // Bloklanarak bir sonraki girdi olayını bekler
        if (ReadConsoleInputW(hIn, &record, 1, &read) && read > 0) {
            // Sadece klavye tuşuna basılma (KeyDown) olaylarını filtrele
            if (record.EventType == KEY_EVENT && record.Event.KeyEvent.bKeyDown) {
                WORD keyCode = record.Event.KeyEvent.wVirtualKeyCode;
                
                SetConsoleMode(hIn, prevMode); // Modu eski haline getir
                
                if (keyCode == VK_UP) return 1;     // Yukarı Ok tuşu için '1' dön
                if (keyCode == VK_DOWN) return 2;   // Aşağı Ok tuşu için '2' dön
                if (keyCode == VK_RETURN) return 3; // Enter tuşu için '3' dön
                if (keyCode == VK_ESCAPE) return 4; // ESC tuşu için '4' dön
            }
        }
    }
}

// Kullanıcıdan maskeli şifre (password) alır
std::wstring ConsoleUI::ReadPassword() {
    std::wstring password;
    while (true) {
        wchar_t ch = _getwch();
        if (ch == L'\r' || ch == L'\n') {
            break;
        } else if (ch == L'\b') { // Backspace (Geri silme)
            if (!password.empty()) {
                password.pop_back();
                std::wcout << L"\b \b" << std::flush;
            }
        } else if (ch == 0 || ch == 0xE0) { // Genişletilmiş tuşlar (ok tuşları vs.)
            _getwch(); // İkinci baytı temizle
        } else {
            password.push_back(ch);
            std::wcout << L"*" << std::flush;
        }
    }
    std::wcout << L"\n" << std::flush;
    return password;
}

// Kullanıcıdan konsol satırı okur. ESC basılırsa false döner (iptal), Enter basılırsa true döner.
bool ConsoleUI::ReadLineInput(std::wstring& out_str) {
    out_str.clear();
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE) return false;

    DWORD prevMode;
    GetConsoleMode(hIn, &prevMode);
    SetConsoleMode(hIn, ENABLE_EXTENDED_FLAGS | ENABLE_PROCESSED_INPUT);

    while (true) {
        wchar_t ch = _getwch();
        if (ch == L'\r' || ch == L'\n') {
            std::wcout << L"\n" << std::flush;
            SetConsoleMode(hIn, prevMode);
            return true;
        } else if (ch == 27) { // ESC tuşu ASCII kodu 27
            std::wcout << L"\n" << std::flush;
            SetConsoleMode(hIn, prevMode);
            return false; // İptal
        } else if (ch == L'\b') { // Backspace
            if (!out_str.empty()) {
                out_str.pop_back();
                std::wcout << L"\b \b" << std::flush;
            }
        } else if (ch == 0 || ch == 0xE0) { // Genişletilmiş tuşlar
            _getwch(); // İkinci baytı temizle
        } else if (ch >= 32) { // Basılabilir karakterler
            out_str.push_back(ch);
            std::wcout << ch << std::flush;
        }
    }
}


