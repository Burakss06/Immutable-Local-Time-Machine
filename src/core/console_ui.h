#pragma once
#include <windows.h>
#include <iostream>
#include <string>

#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif

class ConsoleUI {
public:
    // Windows konsolunda ANSI kaçış dizilerini (renklendirme) aktif eder
    static bool EnableVirtualTerminalProcessing() {
        HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hOut == INVALID_HANDLE_VALUE) return false;

        DWORD dwMode = 0;
        if (!GetConsoleMode(hOut, &dwMode)) return false;

        dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
        if (!SetConsoleMode(hOut, dwMode)) return false;

        return true;
    }

    // Ekranı temizler ve imleci sol üste taşır (Scrollback geçmişini de siler)
    static void ClearScreen() {
        system("cls");
    }

    // Konsol ekranına renkli başlık çizer
    static void DrawHeader() {
        std::wcout << L"\x1B[36m"; // Cyan rengi
        std::wcout << L"==================================================\n";
        std::wcout << L"       IMMUTABLE LOCAL TIME MACHINE (ILTM)        \n";
        std::wcout << L"==================================================\n";
        std::wcout << L"\x1B[0m";  // Renkleri sıfırla
    }

    // Konsol giriş kuyruğundan basılan tuşu okur
    static int ReadMenuInput();
};
