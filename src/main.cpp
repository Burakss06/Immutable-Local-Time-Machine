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

#include <fstream>

// Anahtar okuma ve yazma yardımcı fonksiyonları
inline bool SaveMasterKey(const std::wstring& filepath, const Sha256Hash& key) {
    std::ofstream file(WStringToANSI(filepath), std::ios::binary);
    if (!file.is_open()) return false;
    file.write(reinterpret_cast<const char*>(key.data()), 32);
    return file.good();
}

inline bool LoadMasterKey(const std::wstring& filepath, Sha256Hash& key) {
    std::ifstream file(WStringToANSI(filepath), std::ios::binary);
    if (!file.is_open()) return false;
    file.read(reinterpret_cast<char*>(key.data()), 32);
    return file.gcount() == 32;
}

// Servis durumunu takip eden küresel yapılar
SERVICE_STATUS g_ServiceStatus = {0};
SERVICE_STATUS_HANDLE g_StatusHandle = nullptr;
HANDLE g_ServiceStopEvent = INVALID_HANDLE_VALUE;

// Servis modunda kullanılacak küresel depolama ve sunucu işaretçileri
VaultStorage* g_pStorage = nullptr;
IpcServer* g_pIpcServer = nullptr;

// Servis kontrol sinyal yakalayıcısı
void WINAPI ServiceCtrlHandler(DWORD request) {
    switch (request) {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
            SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
            SetEvent(g_ServiceStopEvent); // Ana döngüyü uyandır
            break;
        default:
            break;
    }
}

// Servis Ana Giriş Noktası
void WINAPI ServiceMain(DWORD argc, LPWSTR* argv) {
    g_StatusHandle = RegisterServiceCtrlHandlerW(L"ILTM_Secure_Service", ServiceCtrlHandler);
    if (!g_StatusHandle) return;

    g_ServiceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_ServiceStatus.dwCurrentState = SERVICE_START_PENDING;
    g_ServiceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    g_ServiceStopEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (g_ServiceStopEvent == nullptr) {
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    // Yerel diskteki master.key dosyasından anahtarı yükle
    Sha256Hash masterKey;
    if (!LoadMasterKey(L"master.key", masterKey)) {
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    // Kasayı ilklendir
    g_pStorage = new VaultStorage(L"vault.bin");
    if (!g_pStorage->Initialize(masterKey)) {
        delete g_pStorage;
        g_pStorage = nullptr;
        g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
        SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
        return;
    }

    // IPC sunucusunu başlat
    g_pIpcServer = new IpcServer(*g_pStorage);
    g_pIpcServer->Start();

    // Servis çalışıyor durumuna getirilir
    g_ServiceStatus.dwCurrentState = SERVICE_RUNNING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    // Servis durdurulana kadar bekler
    WaitForSingleObject(g_ServiceStopEvent, INFINITE);

    // Kapatılıyor
    g_ServiceStatus.dwCurrentState = SERVICE_STOP_PENDING;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);

    if (g_pIpcServer) {
        g_pIpcServer->Stop();
        delete g_pIpcServer;
        g_pIpcServer = nullptr;
    }
    if (g_pStorage) {
        delete g_pStorage;
        g_pStorage = nullptr;
    }

    CloseHandle(g_ServiceStopEvent);

    g_ServiceStatus.dwCurrentState = SERVICE_STOPPED;
    SetServiceStatus(g_StatusHandle, &g_ServiceStatus);
}

// Servisi sisteme kaydeder
bool InstallService() {
    wchar_t path[MAX_PATH];
    if (!GetModuleFileNameW(nullptr, path, MAX_PATH)) return false;
    std::wstring cmd = L"\"" + std::wstring(path) + L"\" --service";

    SC_HANDLE schSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (!schSCManager) return false;

    SC_HANDLE schService = CreateServiceW(
        schSCManager,
        L"ILTM_Secure_Service",
        L"Immutable Local Time Machine",
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        cmd.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr
    );

    if (!schService) {
        CloseServiceHandle(schSCManager);
        return false;
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
}

// Servisi sistemden siler
bool UninstallService() {
    SC_HANDLE schSCManager = OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (!schSCManager) return false;

    SC_HANDLE schService = OpenServiceW(schSCManager, L"ILTM_Secure_Service", DELETE);
    if (!schService) {
        CloseServiceHandle(schSCManager);
        return false;
    }

    if (!DeleteService(schService)) {
        CloseServiceHandle(schService);
        CloseServiceHandle(schSCManager);
        return false;
    }

    CloseServiceHandle(schService);
    CloseServiceHandle(schSCManager);
    return true;
}

int main(int argc, char* argv[]) {
    // Argüman kontrolü
    if (argc > 1) {
        std::string arg = argv[1];
        if (arg == "--install") {
            if (InstallService()) {
                std::wcout << L"[+] Servis basariyla kuruldu: ILTM_Secure_Service" << std::endl;
                return 0;
            } else {
                std::wcerr << L"[-] HATA: Servis kurulurken hata olustu. GetLastError: " << GetLastError() << std::endl;
                return 1;
            }
        } else if (arg == "--uninstall") {
            if (UninstallService()) {
                std::wcout << L"[+] Servis basariyla silindi." << std::endl;
                return 0;
            } else {
                std::wcerr << L"[-] HATA: Servis silinirken hata olustu. GetLastError: " << GetLastError() << std::endl;
                return 1;
            }
        } else if (arg == "--service") {
            // SCM aracılığıyla servis olarak çalıştır
            SERVICE_TABLE_ENTRYW ServiceTable[] = {
                { const_cast<LPWSTR>(L"ILTM_Secure_Service"), (LPSERVICE_MAIN_FUNCTIONW)ServiceMain },
                { nullptr, nullptr }
            };
            if (StartServiceCtrlDispatcherW(ServiceTable)) {
                return 0;
            } else {
                return GetLastError();
            }
        }
    }

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

    // Servisin arka planda okuyabilmesi için anahtar özetini yerel diske kaydet
    SaveMasterKey(L"master.key", masterKey);

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
