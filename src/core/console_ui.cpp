#include "console_ui.h"

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
