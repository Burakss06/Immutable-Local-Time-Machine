#include "core/console_ui.h"
#include "core/watcher.h"
#include "core/vault_storage.h"
#include "core/win32_utils.h"
#include <iostream>
#include <memory>
#include <locale>

void RunWatcher(VaultStorage& storage) {
    ConsoleUI::ClearScreen();
    ConsoleUI::DrawHeader();

    std::wcout << L"Izlenecek dizinin tam yolunu girin:\n> " << std::flush;
    std::wstring watchPath;
    std::getline(std::wcin, watchPath);

    if (watchPath.empty() || !Win32DirectoryExists(watchPath)) {
        std::wcout << L"Gecersiz dizin yolu! Devam etmek icin Enter'a basin...\n" << std::flush;
        std::wstring dummy;
        std::getline(std::wcin, dummy);
        return;
    }

    // Gözcü callback tanımlaması (Observer Pattern)
    auto onFileChange = [&storage](const std::wstring& filePath, FileEvent event) {
        if (event == FileEvent::Added || event == FileEvent::Modified) {
            std::wcout << L"\n[~] Dosya degisikligi algilandi: " << filePath << L"\n";
            std::wcout << L"[~] Bloklar analiz ediliyor ve tekillestiriliyor...\n" << std::flush;
            
            if (storage.BackupFile(filePath)) {
                std::wcout << L"\x1B[32m[+] Blok-seviyesinde basariyla yedeklendi ve tekillestirildi!\x1B[0m\n\n> " << std::flush;
            } else {
                std::wcout << L"\x1B[31m[-] HATA: Dosya kilitli veya okunamadi (Gecici yazma olabilir).\x1B[0m\n\n> " << std::flush;
            }
        }
    };

    DirectoryWatcher watcher(watchPath, onFileChange);
    std::wcout << L"\n[+] Gozcu baslatiliyor...\n" << std::flush;
    if (!watcher.Start()) {
        std::wcout << L"[-] Gozcu baslatilamadi! Devam etmek icin Enter...\n" << std::flush;
        std::wstring dummy;
        std::getline(std::wcin, dummy);
        return;
    }

    std::wcout << L"\x1B[32m[!] Izleme AKTIF. Klasor olaylari dinleniyor.\x1B[0m\n";
    std::wcout << L"\x1B[35m[!] Izlemeyi durdurup ana menuye donmek icin 'ESC' tusuna basin...\x1B[0m\n\n> " << std::flush;

    while (true) {
        int action = ConsoleUI::ReadMenuInput();
        if (action == 4) { // ESC
            break;
        }
    }

    std::wcout << L"\n[!] Gozcu durduruluyor...\n" << std::flush;
    watcher.Stop();
}

void RunRestore(VaultStorage& storage) {
    ConsoleUI::ClearScreen();
    ConsoleUI::DrawHeader();

    std::wcout << L"Geri yuklemek istediginiz dosyanin orijinal tam yolunu girin:\n> " << std::flush;
    std::wstring originPath;
    std::getline(std::wcin, originPath);

    std::wcout << L"Geri yuklenecek yeni dosya yolunu girin (Orn: C:\\restored.txt):\n> " << std::flush;
    std::wstring destPath;
    std::getline(std::wcin, destPath);

    std::wcout << L"\n[~] Geri yukleme (Restore) islemi baslatiliyor...\n" << std::flush;
    
    // Test amacıyla her zaman en son sürümü (Index 0 veya geçmişteki ilk sürümü) çekeceğiz
    if (storage.RestoreFile(originPath, 0, destPath)) {
        std::wcout << L"\x1B[32m[+] Dosya basariyla kasadan cikartildi ve olusturuldu!\x1B[0m\n" << std::flush;
    } else {
        std::wcout << L"\x1B[31m[-] HATA: Dosya kasada bulunamadi veya geri yukleme basarisiz.\x1B[0m\n" << std::flush;
    }

    std::wcout << L"\nDevam etmek icin Enter'a basin..." << std::flush;
    std::wstring dummy;
    std::getline(std::wcin, dummy);
}

int main() {
    try {
        std::locale::global(std::locale(""));
        std::wcout.imbue(std::locale(""));
        std::wcin.imbue(std::locale(""));
    } catch (...) {}

    if (!ConsoleUI::EnableVirtualTerminalProcessing()) {
        std::wcerr << L"HATA: ANSI Konsol modu aktif edilemedi!\n" << std::flush;
    }

    // Kasamızı "vault.bin" dosyasında saklayacağız
    VaultStorage storage(L"vault.bin");
    if (!storage.Initialize()) {
        std::wcerr << L"HATA: Kasa dosya sistemi ilklendirilemedi!\n" << std::flush;
        return 1;
    }

    int selectedItem = 1;
    while (true) {
        ConsoleUI::ClearScreen();
        ConsoleUI::DrawHeader();

        std::wcout << L"\x1B[33mANA MENU\x1B[0m\n";
        std::wcout << (selectedItem == 1 ? L" \x1B[36m-> [1] Dizin Gozcusunu Baslat (Yedekleme Modu)\x1B[0m\n" : L"    [1] Dizin Gozcusunu Baslat (Yedekleme Modu)\n");
        std::wcout << (selectedItem == 2 ? L" \x1B[36m-> [2] Kasa Iceriginden Dosya Geri Yukle (Restore)\x1B[0m\n" : L"    [2] Kasa Iceriginden Dosya Geri Yukle (Restore)\n");
        std::wcout << (selectedItem == 3 ? L" \x1B[36m-> [3] Cikis\x1B[0m\n" : L"    [3] Cikis\n");
        std::wcout << L"\n\x1B[90m(Yon tuslariyla gezinin, Enter ile secin)\x1B[0m\n" << std::flush;

        int key = ConsoleUI::ReadMenuInput();
        if (key == 1) { // Yukari
            selectedItem = (selectedItem == 1) ? 3 : selectedItem - 1;
        } else if (key == 2) { // Asagi
            selectedItem = (selectedItem == 3) ? 1 : selectedItem + 1;
        } else if (key == 3) { // Enter
            if (selectedItem == 1) {
                RunWatcher(storage);
            } else if (selectedItem == 2) {
                RunRestore(storage);
            } else if (selectedItem == 3) {
                break;
            }
        } else if (key == 4) { // ESC
            break;
        }
    }

    ConsoleUI::ClearScreen();
    std::wcout << L"[+] Zaman Makinesi kapatiliyor. Shifu iyi gunler diler!\n" << std::flush;
    return 0;
}
