# ILTM Development Roadmap

## ADIM 1: Özel İkili Kasa Tasarımı (Custom Binary Vault)
- Struct Alignment, Padding ve ham bellek tasarımı.
- `#pragma pack(push, 1)` ile paketlenmiş C++ struct yapılarının tasarımı.

## ADIM 2: Byte Akış Yönetimi (Serialization / Deserialization)
- `std::ofstream::write` ve `std::ifstream::read` metotlarının `reinterpret_cast<const char*>` ile kullanımı.

## ADIM 3: Dosya Bütünlüğü ve İçerik Hash'lemesi (Content Hashing)
- SHA-256 entegrasyonu.

## ADIM 4: Win32 Dizin İzleme Motoru (ReadDirectoryChangesW)
- Windows Asenkron G/Ç mimarisi, HANDLE, OVERLAPPED ve IOCP.

## ADIM 5: Blok Seviyesinde Tekilleştirme (Block-Level Deduplication)
- 4 MB Chunks ve haritalama.

## ADIM 6: RAM İndeksleme ve Arama Motoru
- `std::unordered_map` ile başlayıp Radix Tree'ye geçiş.

## ADIM 7: Kullanıcı Arayüzü Kabuğu (UI)

## ADIM 8: Opsiyonel Hibrit Recovery Modülü Entegrasyonu
