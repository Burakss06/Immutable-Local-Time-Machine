import 'dart:ffi';
import 'dart:typed_data';
import 'package:ffi/ffi.dart';

// Win32 API FFI signatures
typedef CreateFileW_Native = Pointer Function(
  Pointer<Utf16> lpFileName,
  Uint32 dwDesiredAccess,
  Uint32 dwShareMode,
  Pointer lpSecurityAttributes,
  Uint32 dwCreationDisposition,
  Uint32 dwFlagsAndAttributes,
  Pointer hTemplateFile,
);

typedef CreateFileW_Dart = Pointer Function(
  Pointer<Utf16> lpFileName,
  int dwDesiredAccess,
  int dwShareMode,
  Pointer lpSecurityAttributes,
  int dwCreationDisposition,
  int dwFlagsAndAttributes,
  Pointer hTemplateFile,
);

typedef CloseHandle_Native = Int32 Function(Pointer handle);
typedef CloseHandle_Dart = int Function(Pointer handle);

typedef WriteFile_Native = Int32 Function(
  Pointer handle,
  Pointer buffer,
  Uint32 numberOfBytesToWrite,
  Pointer<Uint32> numberOfBytesWritten,
  Pointer overlapped,
);

typedef WriteFile_Dart = int Function(
  Pointer handle,
  Pointer buffer,
  int numberOfBytesToWrite,
  Pointer<Uint32> numberOfBytesWritten,
  Pointer overlapped,
);

typedef ReadFile_Native = Int32 Function(
  Pointer handle,
  Pointer buffer,
  Uint32 numberOfBytesToRead,
  Pointer<Uint32> numberOfBytesRead,
  Pointer overlapped,
);

typedef ReadFile_Dart = int Function(
  Pointer handle,
  Pointer buffer,
  int numberOfBytesToRead,
  Pointer<Uint32> numberOfBytesRead,
  Pointer overlapped,
);

class FileVersion {
  final DateTime timestamp;
  final int blockCount;

  FileVersion(this.timestamp, this.blockCount);

  @override
  String toString() {
    return 'Version{timestamp: $timestamp, blocks: $blockCount}';
  }
}

class IpcBridge {
  static const String pipePath = r'\\.\pipe\ILTM_Secure_Pipe';

  // Constants
  static const int genericRead = 0x80000000;
  static const int genericWrite = 0x40000000;
  static const int openExisting = 3;

  late final CreateFileW_Dart _createFile;
  late final CloseHandle_Dart _closeHandle;
  late final WriteFile_Dart _writeFile;
  late final ReadFile_Dart _readFile;

  Pointer _hPipe = Pointer.fromAddress(0);

  IpcBridge() {
    final kernel32 = DynamicLibrary.open('kernel32.dll');
    _createFile = kernel32.lookupFunction<CreateFileW_Native, CreateFileW_Dart>('CreateFileW');
    _closeHandle = kernel32.lookupFunction<CloseHandle_Native, CloseHandle_Dart>('CloseHandle');
    _writeFile = kernel32.lookupFunction<WriteFile_Native, WriteFile_Dart>('WriteFile');
    _readFile = kernel32.lookupFunction<ReadFile_Native, ReadFile_Dart>('ReadFile');
  }

  bool get isConnected => _hPipe.address != 0 && _hPipe.address != -1 && _hPipe.address != 0xFFFFFFFFFFFFFFFF;

  bool connect() {
    if (isConnected) return true;

    final pPath = pipePath.toNativeUtf16();
    _hPipe = _createFile(
      pPath,
      genericRead | genericWrite,
      0, // No sharing
      nullptr,
      openExisting,
      0,
      nullptr,
    );
    calloc.free(pPath);

    return isConnected;
  }

  void disconnect() {
    if (isConnected) {
      _closeHandle(_hPipe);
      _hPipe = Pointer.fromAddress(0);
    }
  }

  bool _writeBytes(Uint8List bytes) {
    if (!isConnected) return false;

    final pBuffer = calloc<Uint8>(bytes.length);
    pBuffer.asTypedList(bytes.length).setAll(0, bytes);

    final pWritten = calloc<Uint32>();
    final ok = _writeFile(_hPipe, pBuffer, bytes.length, pWritten, nullptr);

    final success = ok != 0 && pWritten.value == bytes.length;
    calloc.free(pBuffer);
    calloc.free(pWritten);
    return success;
  }

  Uint8List? _readBytes(int length) {
    if (!isConnected) return null;

    final pBuffer = calloc<Uint8>(length);
    final pRead = calloc<Uint32>();

    int totalRead = 0;
    while (totalRead < length) {
      final ok = _readFile(
        _hPipe,
        pBuffer.elementAt(totalRead),
        length - totalRead,
        pRead,
        nullptr,
      );

      if (ok == 0 || pRead.value == 0) {
        calloc.free(pBuffer);
        calloc.free(pRead);
        return null;
      }
      totalRead += pRead.value;
    }

    final result = Uint8List.fromList(pBuffer.asTypedList(length));
    calloc.free(pBuffer);
    calloc.free(pRead);
    return result;
  }

  /// Sends a command to the service and reads status code response
  bool sendCommand(int commandId, [Uint8List? payload]) {
    final payloadSize = payload?.length ?? 0;
    final header = Uint8List(12);
    final bd = ByteData.sublistView(header);

    bd.setUint32(0, 0x4950434D, Endian.little); // Magic
    bd.setUint32(4, commandId, Endian.little);
    bd.setUint32(8, payloadSize, Endian.little);

    if (!_writeBytes(header)) return false;
    if (payloadSize > 0 && !_writeBytes(payload!)) return false;

    // Read 12-byte response header
    final respHeader = _readBytes(12);
    if (respHeader == null) return false;

    final rBd = ByteData.sublistView(respHeader);
    final magic = rBd.getUint32(0, Endian.little);
    final command = rBd.getUint32(4, Endian.little);
    final size = rBd.getUint32(8, Endian.little);

    if (magic != 0x4950434D || command != 100 || size != 4) return false;

    // Read 4-byte status code
    final respPayload = _readBytes(4);
    if (respPayload == null) return false;

    final status = ByteData.sublistView(respPayload).getUint32(0, Endian.little);
    return status == 1;
  }

  /// Start watching a folder
  bool startWatcher(String path) {
    final pathUnits = path.codeUnits;
    final payload = Uint8List(4 + pathUnits.length * 2);
    final bd = ByteData.sublistView(payload);

    bd.setUint32(0, pathUnits.length, Endian.little);
    int offset = 4;
    for (final unit in pathUnits) {
      bd.setUint16(offset, unit, Endian.little);
      offset += 2;
    }

    return sendCommand(1, payload); // CMD_START_WATCHER = 1
  }

  /// Stop watching
  bool stopWatcher() {
    return sendCommand(2); // CMD_STOP_WATCHER = 2
  }

  /// Get version history of a file
  List<FileVersion>? getVersions(String path) {
    final pathUnits = path.codeUnits;
    final payload = Uint8List(4 + pathUnits.length * 2);
    final bd = ByteData.sublistView(payload);

    bd.setUint32(0, pathUnits.length, Endian.little);
    int offset = 4;
    for (final unit in pathUnits) {
      bd.setUint16(offset, unit, Endian.little);
      offset += 2;
    }

    // Write request header + payload
    final header = Uint8List(12);
    final hBd = ByteData.sublistView(header);
    hBd.setUint32(0, 0x4950434D, Endian.little);
    hBd.setUint32(4, 3, Endian.little); // CMD_GET_VERSIONS = 3
    hBd.setUint32(8, payload.length, Endian.little);

    if (!_writeBytes(header)) return null;
    if (!_writeBytes(payload)) return null;

    // Read response header
    final respHeader = _readBytes(12);
    if (respHeader == null) return null;

    final rBd = ByteData.sublistView(respHeader);
    final magic = rBd.getUint32(0, Endian.little);
    final command = rBd.getUint32(4, Endian.little);
    final size = rBd.getUint32(8, Endian.little);

    if (magic != 0x4950434D || command != 100 || size < 4) return null;

    final respPayload = _readBytes(size);
    if (respPayload == null) return null;

    final pBd = ByteData.sublistView(respPayload);
    final count = pBd.getUint32(0, Endian.little);

    final versions = <FileVersion>[];
    int pOffset = 4;
    for (int i = 0; i < count; i++) {
      if (pOffset + 12 > size) break;
      final timestampMs = pBd.getUint64(pOffset, Endian.little);
      final blockCount = pBd.getUint32(pOffset + 8, Endian.little);

      versions.add(FileVersion(
        DateTime.fromMillisecondsSinceEpoch(timestampMs),
        blockCount,
      ));
      pOffset += 12;
    }

    return versions;
  }

  /// Restore a version of a file
  bool restore(int versionIndex, String originPath, String destPath) {
    final originUnits = originPath.codeUnits;
    final destUnits = destPath.codeUnits;

    final payloadSize = 4 + 4 + (originUnits.length * 2) + 4 + (destUnits.length * 2);
    final payload = Uint8List(payloadSize);
    final bd = ByteData.sublistView(payload);

    bd.setUint32(0, versionIndex, Endian.little);
    bd.setUint32(4, originUnits.length, Endian.little);

    int offset = 8;
    for (final unit in originUnits) {
      bd.setUint16(offset, unit, Endian.little);
      offset += 2;
    }

    bd.setUint32(offset, destUnits.length, Endian.little);
    offset += 4;

    for (final unit in destUnits) {
      bd.setUint16(offset, unit, Endian.little);
      offset += 2;
    }

    return sendCommand(4, payload); // CMD_RESTORE = 4
  }
}
