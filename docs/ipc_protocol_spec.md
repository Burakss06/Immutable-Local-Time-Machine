# ILTM Named Pipe IPC Protokol Spesifikasyonu

Bu belge, **Immutable Local Time Machine (ILTM)** arka plan servisinin (`iltm.exe`) arayüz kabukları (Electron, Flutter, Angular vb.) ile `\\.\pipe\ILTM_Secure_Pipe` isimlendirilmiş borusu (Named Pipe) üzerinden nasıl haberleşeceğini tanımlar.

---

## 1. Paket Yapısı (Binary Packet Layout)

Tüm komutlar ve yanıtlar sıkıştırılmış bayt hizalaması (`#pragma pack(push, 1)`) kullanan 12 byte'lık bir başlık (Header) ve ardından gelen değişken uzunluktaki veriden (Payload) oluşur.

```
+-------------------+--------------------+----------------------+-----------------------------+
|  Magic (4 Byte)   |  Command ID (4B)   |  Payload Size (4B)   |   Payload Data (Var Bytes)  |
+-------------------+--------------------+----------------------+-----------------------------+
|    0x4950434D     |  1, 2, 3, 4 veya   |    N (Byte Boyutu)   |  Yollar, indisler veya      |
|   ('IPCM' ASCII)  |  100 (Response)    |                      |  durum verileri             |
+-------------------+--------------------+----------------------+-----------------------------+
```

### Komut ID'leri (Command IDs):
* `1` = `CMD_START_WATCHER` (Dizin izlemeyi başlat)
* `2` = `CMD_STOP_WATCHER` (Dizin izlemeyi durdur)
* `3` = `CMD_GET_VERSIONS` (Dosyanın sürüm geçmişini al)
* `4` = `CMD_RESTORE` (Dosyayı geri yükle)
* `100` = `CMD_RESPONSE` (Sunucunun istemciye yanıtı)

---

## 2. Komut Detayları ve Payload Düzenleri

### A. CMD_START_WATCHER (ID: 1)
Servisin bir klasörü asenkron izlemeye almasını sağlar.
* **Payload Düzeni:**
  - `4 Byte (uint32_t)`: Klasör yolu karakter sayısı (L)
  - `L * 2 Byte (wchar_t[])`: UTF-16 (Unicode) Klasör Yolu
* **Sunucu Yanıtı:**
  - `CMD_RESPONSE` başlığı altında `4 Byte (uint32_t)` durum kodu döner. (`1` = Başarılı, `0` = Hata).

---

### B. CMD_STOP_WATCHER (ID: 2)
Aktif izleme işlemini durdurur.
* **Payload Düzeni:** Yokdur (Payload Size = 0).
* **Sunucu Yanıtı:**
  - `CMD_RESPONSE` başlığı altında `4 Byte (uint32_t)` durum kodu döner (`1` = Durduruldu).

---

### C. CMD_GET_VERSIONS (ID: 3)
Belirtilen dosyanın kasa içindeki yedekleme geçmişini sorgular.
* **Payload Düzeni:**
  - `4 Byte (uint32_t)`: Dosya yolu karakter sayısı (L)
  - `L * 2 Byte (wchar_t[])`: UTF-16 (Unicode) Orijinal Dosya Yolu
* **Sunucu Yanıtı (Serileştirilmiş Sürüm Listesi):**
  - `4 Byte (uint32_t)`: Toplam sürüm sayısı (C)
  - Eğer `C > 0` ise, ardışık her bir sürüm için:
    - `8 Byte (uint64_t)`: Zaman damgası (Epoch milisaniye)
    - `4 Byte (uint32_t)`: Dosyayı oluşturan blok sayısı (deduplicated block count)

---

### D. CMD_RESTORE (ID: 4)
Dosyanın belirli bir sürümünü hedef yola deşifre edip çıkartır.
* **Payload Düzeni:**
  - `4 Byte (uint32_t)`: Geri yüklenecek sürüm indisi (0-based index)
  - `4 Byte (uint32_t)`: Orijinal dosya yolu karakter sayısı (OL)
  - `OL * 2 Byte (wchar_t[])`: UTF-16 Orijinal Dosya Yolu
  - `4 Byte (uint32_t)`: Hedef dosya yolu karakter sayısı (DL)
  - `DL * 2 Byte (wchar_t[])`: UTF-16 Hedef Dosya Yolu
* **Sunucu Yanıtı:**
  - `CMD_RESPONSE` başlığı altında `4 Byte (uint32_t)` durum kodu döner (`1` = Geri Yüklendi, `0` = Hata).

---

## 3. Arayüz Entegrasyon Kod Örnekleri

### Node.js (Electron / Angular İçin)
Node.js üzerinde `net` kütüphanesini kullanarak boruya bağlanmak ve sürüm listesi sorgulamak için örnek kod:

```javascript
const net = require('net');

const PIPE_PATH = '\\\\.\\pipe\\ILTM_Secure_Pipe';
const client = net.createConnection(PIPE_PATH, () => {
    console.log('ILTM Servisine baglanildi.');
    
    // CMD_GET_VERSIONS talebi hazırla (C:\test.txt için)
    const filePath = 'C:\\test.txt';
    const filePathBuffer = Buffer.from(filePath, 'utf16le');
    
    const header = Buffer.alloc(12);
    header.writeUInt32LE(0x4950434M, 0); // Magic
    header.writeUInt32LE(3, 4);          // Command ID (CMD_GET_VERSIONS)
    header.writeUInt32LE(4 + filePathBuffer.length, 8); // Payload Size
    
    const payload = Buffer.alloc(4 + filePathBuffer.length);
    payload.writeUInt32LE(filePath.length, 0);
    filePathBuffer.copy(payload, 4);
    
    // Sunucuya paketi gönder
    client.write(Buffer.concat([header, payload]));
});

client.on('data', (data) => {
    // Yanıt başlığını ayrıştır
    const magic = data.readUInt32LE(0);
    const command = data.readUInt32LE(4);
    const payloadSize = data.readUInt32LE(8);
    
    if (magic === 0x4950434M && command === 100) { // CMD_RESPONSE
        const versionCount = data.readUInt32LE(12);
        console.log(`Bulunan Surum Sayisi: ${versionCount}`);
        
        let offset = 16;
        for (let i = 0; i < versionCount; i++) {
            // BigInt olarak zaman damgası okunur
            const timestamp = data.readBigUInt64LE(offset);
            const blockCount = data.readUInt32LE(offset + 8);
            
            const date = new Date(Number(timestamp));
            console.log(`  Sürüm ${i + 1}: Tarih: ${date.toLocaleString()}, Blok: ${blockCount}`);
            offset += 12;
        }
    }
    client.end();
});
```

### Dart / Flutter
Flutter masaüstü uygulamasında binary protokolü Named Pipe ile okumak için örnek akış şeması:

```dart
import 'dart:io';
import 'dart:typed_data';

void getFileHistory() async {
  // Windows Named Pipe Dart dosya sistemi üzerinden açılır
  final socket = await Socket.connect(r'\\.\pipe\ILTM_Secure_Pipe', 0);
  
  final path = r'C:\test.txt';
  final pathBytes = Uint8List.fromList(path.codeUnits); // UTF-16 formatında dizgi
  
  // Paket tamponu oluşturulur
  final header = ByteData(12);
  header.setUint32(0, 0x4950434M, Endian.little);
  header.setUint32(4, 3, Endian.little); // CMD_GET_VERSIONS
  header.setUint32(8, 4 + (path.length * 2), Endian.little);
  
  final payload = ByteData(4);
  payload.setUint32(0, path.length, Endian.little);
  
  // Boruya yazarız
  socket.add(header.buffer.asUint8List());
  socket.add(payload.buffer.asUint8List());
  socket.add(pathBytes);
  
  await socket.flush();
  
  // Yanıt dinlenir
  socket.listen((Uint8List responseData) {
    final respHeader = ByteData.sublistView(responseData, 0, 12);
    final magic = respHeader.getUint32(0, Endian.little);
    final command = respHeader.getUint32(4, Endian.little);
    
    if (magic == 0x4950434M && command == 100) {
      final payloadData = ByteData.sublistView(responseData, 12);
      final count = payloadData.getUint32(0, Endian.little);
      print("Toplam Sürüm: $count");
    }
    socket.close();
  });
}
```
