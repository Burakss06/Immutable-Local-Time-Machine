#include "core/console_ui.h"
#include "core/watcher.h"
#include "core/vault_storage.h"
#include "core/win32_utils.h"
#include "core/crypto.h"
#include "core/ipc_server.h"
#include <iostream>
#include <memory>
#include <locale>

void RunWatcher(VaultStorage& storage) {
    ConsoleUI::ClearScreen();
    ConsoleUI::DrawHeader();

    std::wcout << L"Izlenecek dizinin tam yolunu girin (Iptal icin ESC):\n> " << std::flush;
    std::wstring watchPath;
    if (!ConsoleUI::ReadLineInput(watchPath)) {
        return; // ESC ile çıkış
    }

    if (watchPath.empty() || !Win32DirectoryExists(watchPath)) {
        std::wcout << L"Gecersiz dizin yolu! Devam etmek icin Enter'a basin...\n" << std::flush;
        std::wstring dummy;
        ConsoleUI::ReadLineInput(dummy);
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
        ConsoleUI::ReadLineInput(dummy);
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

#include <ctime>

// Zaman damgasını insan tarafından okunabilir tarihe çevirir
inline std::wstring FormatTimestamp(uint64_t ms) {
    time_t secs = static_cast<time_t>(ms / 1000);
    std::tm* t = std::localtime(&secs);
    if (!t) return L"Bilinmeyen Zaman";
    wchar_t buf[64];
    wcsftime(buf, 64, L"%d.%m.%Y %H:%M:%S", t);
    return std::wstring(buf);
}

void RunRestore(VaultStorage& storage) {
    ConsoleUI::ClearScreen();
    ConsoleUI::DrawHeader();

    std::wcout << L"Geri yuklemek istediginiz dosyanin orijinal tam yolunu girin (Iptal icin ESC):\n> " << std::flush;
    std::wstring originPath;
    if (!ConsoleUI::ReadLineInput(originPath)) {
        return; // ESC ile çıkış
    }

    // Dosyanın kasa içindeki geçmiş sürümlerini sorgula
    const auto* history = storage.GetFileHistory(originPath);
    if (!history || history->empty()) {
        std::wcout << L"\n\x1B[31m[-] HATA: Bu dosya icin kayitli hicbir surum bulunamadi!\x1B[0m\n";
        std::wcout << L"\nDevam etmek icin Enter'a basin..." << std::flush;
        std::wstring dummy;
        ConsoleUI::ReadLineInput(dummy);
        return;
    }

    int selectedVersion = 0;
    int versionCount = static_cast<int>(history->size());

    // Sürüm Seçim Menüsü Döngüsü
    while (true) {
        ConsoleUI::ClearScreen();
        ConsoleUI::DrawHeader();
        std::wcout << L"\x1B[33mSURUM GECMISI MENUSU\x1B[0m\n";
        std::wcout << L"Dosya: " << originPath << L"\n\n";

        for (int i = 0; i < versionCount; ++i) {
            const auto& ver = (*history)[i];
            std::wstring timeStr = FormatTimestamp(ver.timestamp);
            if (i == selectedVersion) {
                std::wcout << L" \x1B[36m-> [" << i + 1 << L"] Surum: " << timeStr 
                           << L" (" << ver.blockHashes.size() << L" Blok)\x1B[0m\n";
            } else {
                std::wcout << L"    [" << i + 1 << L"] Surum: " << timeStr 
                           << L" (" << ver.blockHashes.size() << L" Blok)\n";
            }
        }
        std::wcout << L"\n\x1B[90m(Yon tuslariyla secin, Enter ile geri yukleyin, ESC ile iptal edin)\x1B[0m\n" << std::flush;

        int key = ConsoleUI::ReadMenuInput();
        if (key == 1) { // Yukarı
            selectedVersion = (selectedVersion == 0) ? versionCount - 1 : selectedVersion - 1;
        } else if (key == 2) { // Aşağı
            selectedVersion = (selectedVersion == versionCount - 1) ? 0 : selectedVersion + 1;
        } else if (key == 3) { // Enter
            break;
        } else if (key == 4) { // ESC
            return;
        }
    }

    ConsoleUI::ClearScreen();
    ConsoleUI::DrawHeader();
    std::wcout << L"Geri yuklenecek yeni dosya yolunu girin (Orn: C:\\restored.txt, Iptal icin ESC):\n> " << std::flush;
    std::wstring destPath;
    if (!ConsoleUI::ReadLineInput(destPath)) {
        return; // ESC ile çıkış
    }

    std::wcout << L"\n[~] Geri yukleme (Restore) islemi baslatiliyor...\n" << std::flush;
    
    if (storage.RestoreFile(originPath, selectedVersion, destPath)) {
        std::wcout << L"\x1B[32m[+] Dosya basariyla kasadan cikartildi ve olusturuldu!\x1B[0m\n" << std::flush;
    } else {
        std::wcout << L"\x1B[31m[-] HATA: Geri yukleme basarisiz.\x1B[0m\n" << std::flush;
    }

    std::wcout << L"\nDevam etmek icin Enter'a basin..." << std::flush;
    std::wstring dummy;
    ConsoleUI::ReadLineInput(dummy);
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

    ConsoleUI::ClearScreen();
    ConsoleUI::DrawHeader();

    std::wcout << L"Lutfen Kasa Sifresini Girin:\n> " << std::flush;
    std::wstring password = ConsoleUI::ReadPassword();
    if (password.empty()) {
        std::wcerr << L"HATA: Sifre bos olamaz!\n" << std::flush;
        return 1;
    }

    Sha256Hash masterKey;
    if (!CalculateSHA256(reinterpret_cast<const uint8_t*>(password.data()), password.size() * sizeof(wchar_t), masterKey)) {
        std::wcerr << L"HATA: MasterKey uretilemedi!\n" << std::flush;
        return 1;
    }

    // Kasamızı "vault.bin" dosyasında saklayacağız
    VaultStorage storage(L"vault.bin");
    if (!storage.Initialize(masterKey)) {
        std::wcerr << L"HATA: Kasa dosya sistemi ilklendirilemedi!\n" << std::flush;
        return 1;
    }

    // Asenkron IPC sunucusunu arka planda ateşle
    IpcServer ipcServer(storage);
    ipcServer.Start();

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

    ipcServer.Stop(); // IPC sunucusunu kapat ve temiz kapatma yap

    ConsoleUI::ClearScreen();
    std::wcout << L"[+] Zaman Makinesi kapatiliyor. Shifu iyi gunler diler!\n" << std::flush;
    return 0;
}
