import 'dart:async';
import 'dart:convert';
import 'dart:io';
import 'package:flutter/material.dart';
import 'ipc_bridge.dart';

void main() {
  runApp(const IltmApp());
}

class IltmApp extends StatelessWidget {
  const IltmApp({super.key});

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'ILTM Dashboard',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        scaffoldBackgroundColor: const Color(0xFF090A0F),
        colorScheme: const ColorScheme.dark(
          primary: Color(0xFF00FF87), // Neon Green
          secondary: Color(0xFF00E5FF), // Neon Cyan
          surface: Color(0xFF121420),
          error: Color(0xFFFF3366),
        ),
        textTheme: const TextTheme(
          titleLarge: TextStyle(fontFamily: 'Inter', fontWeight: FontWeight.w700, letterSpacing: 0.5),
          bodyMedium: TextStyle(fontFamily: 'Inter', color: Color(0xFFA0A5C0)),
        ),
      ),
      home: const DashboardPage(),
    );
  }
}

class DashboardPage extends StatefulWidget {
  const DashboardPage({super.key});

  @override
  State<DashboardPage> createState() => _DashboardPageState();
}

class _DashboardPageState extends State<DashboardPage> {
  final IpcBridge _bridge = IpcBridge();
  final TextEditingController _pathController = TextEditingController(text: r'C:\test.txt');
  final TextEditingController _watchPathController = TextEditingController(text: r'C:\test_folder');
  final TextEditingController _ruleController = TextEditingController(text: r'+*.cpp');
  
  bool _connected = false;
  bool _isWatching = false;
  bool _panicState = false;
  String _panicFilePath = "";
  List<FileVersion> _history = [];
  List<String> _rules = [];
  String _statusMessage = 'Servise bağlanılmadı.';
  Timer? _statusTimer;

  int? _selectedVersionIndex;
  List<String> _oldVersionLines = [];
  List<String> _currentFileLines = [];

  @override
  void initState() {
    super.initState();
    _tryConnect();
  }

  @override
  void dispose() {
    _statusTimer?.cancel();
    super.dispose();
  }

  void _tryConnect() {
    print('[GUI-DEBUG] _tryConnect called. Invoking _bridge.connect()...');
    setState(() {
      _connected = _bridge.connect();
      print('[GUI-DEBUG] _bridge.connect() returned: $_connected');
      if (_connected) {
        _statusMessage = 'Servis Bağlantısı Aktif.';
        _statusTimer?.cancel();
        _statusTimer = Timer.periodic(const Duration(milliseconds: 1000), (timer) {
          _checkStatus();
        });
        _checkStatus();
      } else {
        _statusMessage = 'Bağlantı hatası! iltm.exe servisinin çalıştığından emin olun.';
      }
    });
  }

  void _checkStatus() {
    print('[GUI-DEBUG] _checkStatus polling... Invoking _bridge.getStatus()...');
    final info = _bridge.getStatus();
    if (info == null) {
      print('[GUI-DEBUG] _bridge.getStatus() returned NULL! Connection dropping.');
      setState(() {
        _connected = false;
        _statusMessage = 'Servis bağlantısı koptu.';
        _statusTimer?.cancel();
      });
      return;
    }

    print('[GUI-DEBUG] _bridge.getStatus() returned: state=${info.state}, watchPath=${info.watchPath}, panicPath=${info.panicPath}');
    setState(() {
      _connected = true;
      _isWatching = info.state == 1;
      _panicState = info.state == 2;
      _panicFilePath = info.panicPath;
      if (_panicState) {
        _statusMessage = 'KRİTİK HATA: Fidye Yazılımı Saldırısı Algılandı!';
      } else if (_isWatching) {
        _statusMessage = 'İzleme Aktif: ${info.watchPath}';
      } else {
        _statusMessage = 'Servis Bağlantısı Aktif. Gözcü boşta.';
      }
    });
  }

  void _disconnect() {
    _statusTimer?.cancel();
    _bridge.disconnect();
    setState(() {
      _connected = false;
      _history.clear();
      _isWatching = false;
      _panicState = false;
      _statusMessage = 'Bağlantı kesildi.';
    });
  }

  void _queryHistory() {
    if (!_connected) return;
    final path = _pathController.text.trim();
    if (path.isEmpty) return;

    final versions = _bridge.getVersions(path);
    setState(() {
      _selectedVersionIndex = null;
      _oldVersionLines.clear();
      _currentFileLines.clear();
      if (versions == null) {
        _history = [];
        _statusMessage = 'Sürüm geçmişi alınamadı veya dosya bulunamadı.';
      } else {
        _history = versions;
        _statusMessage = 'Sürümler başarıyla yüklendi. Bulunan: ${versions.length}';
      }
    });
  }

  void _toggleWatcher() {
    if (!_connected) return;
    if (_isWatching || _panicState) {
      final ok = _bridge.stopWatcher();
      if (ok) {
        setState(() {
          _isWatching = false;
          _panicState = false;
          _statusMessage = 'İzleme durduruldu, panik durumu sıfırlandı.';
        });
      } else {
        setState(() => _statusMessage = 'Gözcü durdurulurken hata oluştu.');
      }
    } else {
      final path = _watchPathController.text.trim();
      if (path.isEmpty) return;
      final ok = _bridge.startWatcher(path);
      if (ok) {
        setState(() {
          _isWatching = true;
          _statusMessage = 'Dizin izleme başlatıldı: $path';
        });
      } else {
        setState(() => _statusMessage = 'Gözcü başlatılırken hata oluştu.');
      }
    }
  }

  void _addRule() {
    final rule = _ruleController.text.trim();
    if (rule.isEmpty) return;
    setState(() {
      _rules.add(rule);
      _ruleController.clear();
    });
    _applyRules();
  }

  void _removeRule(int index) {
    setState(() {
      _rules.removeAt(index);
    });
    _applyRules();
  }

  void _applyRules() {
    if (!_connected) return;
    final ok = _bridge.setRules(_rules);
    setState(() {
      _statusMessage = ok ? 'Glob filtre kuralları uygulandı.' : 'Kurallar uygulanamadı!';
    });
  }

  void _restoreVersion(int index) {
    if (!_connected) return;
    final path = _pathController.text.trim();
    final ok = _bridge.restore(index, path, path);
    setState(() {
      _statusMessage = ok 
          ? 'Sürüm $index başarıyla geri yüklendi!' 
          : 'Geri yükleme hatası!';
    });
  }

  void _loadDiffPreview(int index) {
    final path = _pathController.text.trim();
    if (path.isEmpty) return;

    final bytes = _bridge.getVersionContent(index, path);
    if (bytes == null) {
      setState(() => _statusMessage = 'Sürüm içeriği çözülemedi.');
      return;
    }

    // Tarihsel içerik
    final oldText = utf8.decode(bytes, allowMalformed: true);
    
    // Güncel içerik
    String currentText = "";
    try {
      final file = File(path);
      if (file.existsSync()) {
        currentText = file.readAsStringSync();
      } else {
        currentText = "[Dosya şu an diskte mevcut değil]";
      }
    } catch (e) {
      currentText = "[Dosya okuma hatası: $e]";
    }

    setState(() {
      _selectedVersionIndex = index;
      _oldVersionLines = oldText.split('\n');
      _currentFileLines = currentText.split('\n');
    });
  }

  @override
  Widget build(BuildContext context) {
    // Ransomware Panic Overlay Screen
    if (_panicState) {
      return Scaffold(
        backgroundColor: const Color(0xFF2D0913),
        body: Center(
          child: Container(
            constraints: const BoxConstraints(maxWidth: 600),
            padding: const EdgeInsets.all(40),
            decoration: BoxDecoration(
              color: const Color(0xFF181A2A),
              borderRadius: BorderRadius.circular(16),
              border: Border.all(color: Colors.redAccent, width: 2),
            ),
            child: Column(
              mainAxisSize: MainAxisSize.min,
              children: [
                const Icon(Icons.gpp_bad, color: Colors.redAccent, size: 72),
                const SizedBox(height: 24),
                const Text(
                  'FİDYE YAZILIMI ALARMI!',
                  style: TextStyle(fontSize: 24, fontWeight: FontWeight.bold, color: Colors.redAccent),
                ),
                const SizedBox(height: 16),
                const Text(
                  'Sistem, hedef dizinde hızlı ve yüksek entropili (şifreli) dosya yazma etkinlikleri tespit etti. Kasa verilerini korumak için izleme servisi otomatik kilitlendi.',
                  textAlign: TextAlign.center,
                  style: TextStyle(color: Colors.white70),
                ),
                const SizedBox(height: 16),
                Text(
                  'Hedef Tehdit Dosyası:\n$_panicFilePath',
                  textAlign: TextAlign.center,
                  style: const TextStyle(fontFamily: 'Consolas', color: Colors.amberAccent, fontSize: 13),
                ),
                const SizedBox(height: 32),
                ElevatedButton(
                  onPressed: _toggleWatcher, // stopWatcher fits panic clear
                  style: ElevatedButton.styleFrom(
                    backgroundColor: Colors.redAccent,
                    foregroundColor: Colors.white,
                    padding: const EdgeInsets.symmetric(horizontal: 24, vertical: 16),
                  ),
                  child: const Text('Tehdidi Temizle & Servisi Sıfırla'),
                ),
              ],
            ),
          ),
        ),
      );
    }

    return Scaffold(
      body: Row(
        children: [
          // Left Sidebar (Controls & Filtering)
          Container(
            width: 320,
            color: const Color(0xFF0F111E),
            padding: const EdgeInsets.all(20.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    const Icon(Icons.lock_clock, color: Color(0xFF00FF87), size: 28),
                    const SizedBox(width: 12),
                    Text(
                      'ILTM SHIELD',
                      style: Theme.of(context).textTheme.titleLarge?.copyWith(fontSize: 18),
                    ),
                  ],
                ),
                const SizedBox(height: 24),
                
                // Connection Card
                Container(
                  padding: const EdgeInsets.all(12),
                  decoration: BoxDecoration(
                    color: const Color(0xFF181A2A),
                    borderRadius: BorderRadius.circular(10),
                    border: Border.all(color: const Color(0xFF262943)),
                  ),
                  child: Column(
                    children: [
                      Row(
                        mainAxisAlignment: MainAxisAlignment.spaceBetween,
                        children: [
                          Text('Servis Durumu:', style: TextStyle(fontSize: 12, color: Colors.grey[400])),
                          Container(
                            width: 10,
                            height: 10,
                            decoration: BoxDecoration(
                              shape: BoxShape.circle,
                              color: _connected ? const Color(0xFF00FF87) : Colors.redAccent,
                              boxShadow: [
                                BoxShadow(
                                  color: _connected ? const Color(0xFF00FF87).withOpacity(0.5) : Colors.redAccent.withOpacity(0.5),
                                  blurRadius: 6,
                                )
                              ],
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 12),
                      Row(
                        children: [
                          Expanded(
                            child: ElevatedButton(
                              onPressed: _connected ? null : _tryConnect,
                              style: ElevatedButton.styleFrom(
                                backgroundColor: const Color(0xFF00FF87),
                                foregroundColor: Colors.black,
                              ),
                              child: const Text('Bağlan'),
                            ),
                          ),
                          const SizedBox(width: 8),
                          Expanded(
                            child: OutlinedButton(
                              onPressed: _connected ? _disconnect : null,
                              child: const Text('Kapat'),
                            ),
                          ),
                        ],
                      )
                    ],
                  ),
                ),
                const SizedBox(height: 24),
                
                // Watcher Panel
                Text('DİZİN İZLEYİCİ', style: TextStyle(fontSize: 11, fontWeight: FontWeight.bold, color: Colors.grey[500], letterSpacing: 1)),
                const SizedBox(height: 8),
                TextField(
                  controller: _watchPathController,
                  decoration: const InputDecoration(
                    labelText: 'İzlenecek Klasör',
                    filled: true,
                    fillColor: Color(0xFF181A2A),
                    border: OutlineInputBorder(),
                    contentPadding: EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                  ),
                ),
                const SizedBox(height: 8),
                SizedBox(
                  width: double.infinity,
                  child: ElevatedButton(
                    onPressed: _connected ? _toggleWatcher : null,
                    style: ElevatedButton.styleFrom(
                      backgroundColor: _isWatching ? Colors.amber[800] : const Color(0xFF00E5FF),
                      foregroundColor: Colors.black,
                    ),
                    child: Text(_isWatching ? 'Gözcüyü Durdur' : 'İzlemeyi Başlat'),
                  ),
                ),
                const SizedBox(height: 24),

                // Glob Rule Filtering Panel
                Text('AKILLI FILTRELEME (GLOB)', style: TextStyle(fontSize: 11, fontWeight: FontWeight.bold, color: Colors.grey[500], letterSpacing: 1)),
                const SizedBox(height: 8),
                Row(
                  children: [
                    Expanded(
                      child: TextField(
                        controller: _ruleController,
                        decoration: const InputDecoration(
                          hintText: 'örn: +*.cpp veya -*.tmp',
                          filled: true,
                          fillColor: Color(0xFF181A2A),
                          border: OutlineInputBorder(),
                          contentPadding: EdgeInsets.symmetric(horizontal: 12, vertical: 8),
                        ),
                      ),
                    ),
                    const SizedBox(width: 8),
                    IconButton(
                      icon: const Icon(Icons.add_circle, color: Color(0xFF00FF87)),
                      onPressed: _connected ? _addRule : null,
                    )
                  ],
                ),
                const SizedBox(height: 12),
                Expanded(
                  child: Container(
                    decoration: BoxDecoration(
                      color: const Color(0xFF121420),
                      borderRadius: BorderRadius.circular(8),
                    ),
                    child: ListView.builder(
                      itemCount: _rules.length,
                      itemBuilder: (context, index) {
                        final rule = _rules[index];
                        final isExclude = rule.startsWith('-');
                        return ListTile(
                          title: Text(
                            rule,
                            style: TextStyle(
                              fontFamily: 'Consolas',
                              fontSize: 12,
                              color: isExclude ? Colors.redAccent : const Color(0xFF00FF87),
                            ),
                          ),
                          trailing: IconButton(
                            icon: const Icon(Icons.cancel, size: 16, color: Colors.white38),
                            onPressed: () => _removeRule(index),
                          ),
                          dense: true,
                          contentPadding: const EdgeInsets.symmetric(horizontal: 8),
                        );
                      },
                    ),
                  ),
                ),
                const SizedBox(height: 12),
                Text(
                  _statusMessage,
                  style: const TextStyle(fontSize: 10, color: Color(0xFF00E5FF)),
                ),
              ],
            ),
          ),
          
          // Right Main Pane
          Expanded(
            child: Container(
              padding: const EdgeInsets.all(32),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  // File selection header
                  Row(
                    children: [
                      Expanded(
                        child: TextField(
                          controller: _pathController,
                          decoration: const InputDecoration(
                            labelText: 'Sorgulanacak Orijinal Dosya Yolu',
                            filled: true,
                            fillColor: Color(0xFF0F111E),
                            border: OutlineInputBorder(),
                          ),
                        ),
                      ),
                      const SizedBox(width: 16),
                      SizedBox(
                        height: 56,
                        width: 130,
                        child: ElevatedButton.icon(
                          onPressed: _connected ? _queryHistory : null,
                          icon: const Icon(Icons.history),
                          label: const Text('Geçmiş'),
                        ),
                      )
                    ],
                  ),
                  const SizedBox(height: 24),
                  
                  // Visual Scrubbing Split Pane (Left: Timeline / Right: Visual Diff)
                  Expanded(
                    child: Row(
                      crossAxisAlignment: CrossAxisAlignment.stretch,
                      children: [
                        // Left: Version Timeline
                        Expanded(
                          flex: 2,
                          child: _history.isEmpty
                              ? Center(
                                  child: Text(
                                    'Kasa geçmişi sorgusu bekleniyor.',
                                    style: TextStyle(color: Colors.grey[600]),
                                  ),
                                )
                              : ListView.builder(
                                  itemCount: _history.length,
                                  itemBuilder: (context, index) {
                                    final ver = _history[index];
                                    final isSelected = _selectedVersionIndex == index;
                                    return Card(
                                      color: isSelected ? const Color(0xFF1F2235) : const Color(0xFF0F111E),
                                      margin: const EdgeInsets.only(bottom: 12),
                                      shape: RoundedRectangleBorder(
                                        borderRadius: BorderRadius.circular(8),
                                        side: BorderSide(
                                          color: isSelected ? const Color(0xFF00E5FF) : const Color(0xFF262943),
                                          width: isSelected ? 1.5 : 1.0,
                                        ),
                                      ),
                                      child: ListTile(
                                        onTap: () => _loadDiffPreview(index),
                                        leading: CircleAvatar(
                                          backgroundColor: isSelected ? const Color(0xFF00E5FF).withOpacity(0.2) : const Color(0xFF1F2235),
                                          child: Icon(Icons.history, color: isSelected ? const Color(0xFF00E5FF) : const Color(0xFF00FF87)),
                                        ),
                                        title: Text('Sürüm #${index + 1}', style: const TextStyle(fontWeight: FontWeight.bold)),
                                        subtitle: Text(
                                          'Tarih: ${ver.timestamp.toLocal().toString().split('.')[0]}\nBlok: ${ver.blockCount} (Deltalar)',
                                          style: const TextStyle(fontSize: 12),
                                        ),
                                        isThreeLine: true,
                                        trailing: ElevatedButton(
                                          onPressed: () => _restoreVersion(index),
                                          style: ElevatedButton.styleFrom(
                                            backgroundColor: const Color(0xFF181A2A),
                                          ),
                                          child: const Text('Kurtar'),
                                        ),
                                      ),
                                    );
                                  },
                                ),
                        ),
                        const SizedBox(width: 20),
                        
                        // Right: Visual Diff Viewer
                        Expanded(
                          flex: 3,
                          child: _selectedVersionIndex == null
                              ? Container(
                                  decoration: BoxDecoration(
                                    color: const Color(0xFF0F111E),
                                    borderRadius: BorderRadius.circular(10),
                                    border: Border.all(color: const Color(0xFF262943)),
                                  ),
                                  child: Center(
                                    child: Column(
                                      mainAxisAlignment: MainAxisAlignment.center,
                                      children: [
                                        Icon(Icons.compare, size: 48, color: Colors.grey[700]),
                                        const SizedBox(height: 12),
                                        Text(
                                          'Görsel Zaman Tüneli Karşılaştırıcı\n(Sürüm seçtiğinizde burası farkları gösterir)',
                                          textAlign: TextAlign.center,
                                          style: TextStyle(color: Colors.grey[600]),
                                        )
                                      ],
                                    ),
                                  ),
                                )
                              : Container(
                                  decoration: BoxDecoration(
                                    color: const Color(0xFF0F111E),
                                    borderRadius: BorderRadius.circular(10),
                                    border: Border.all(color: const Color(0xFF262943)),
                                  ),
                                  child: Column(
                                    crossAxisAlignment: CrossAxisAlignment.stretch,
                                    children: [
                                      // Diff Header
                                      Container(
                                        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
                                        color: const Color(0xFF181A2A),
                                        child: Text(
                                          'FARKLILIK GÖSTERİCİ: Sürüm #${_selectedVersionIndex! + 1} vs Mevcut Sürüm',
                                          style: const TextStyle(fontSize: 12, fontWeight: FontWeight.bold, color: Color(0xFF00E5FF)),
                                        ),
                                      ),
                                      
                                      // Side-by-Side Diff Panels
                                      Expanded(
                                        child: Row(
                                          children: [
                                            // Historic Version View
                                            Expanded(
                                              child: _buildDiffColumn('SÜRÜM #${_selectedVersionIndex! + 1} (Tarihsel)', _oldVersionLines, true),
                                            ),
                                            const VerticalDivider(width: 1, color: Color(0xFF262943)),
                                            // Current Version View
                                            Expanded(
                                              child: _buildDiffColumn('DİSKTEKİ GÜNCEL HALİ', _currentFileLines, false),
                                            ),
                                          ],
                                        ),
                                      )
                                    ],
                                  ),
                                ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          )
        ],
      ),
    );
  }

  Widget _buildDiffColumn(String title, List<String> lines, bool isOld) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.stretch,
      children: [
        Padding(
          padding: const EdgeInsets.all(8.0),
          child: Text(title, style: const TextStyle(fontSize: 11, fontWeight: FontWeight.bold, color: Colors.white60)),
        ),
        Expanded(
          child: Container(
            color: const Color(0xFF090A0F),
            child: ListView.builder(
              itemCount: lines.length,
              itemBuilder: (context, idx) {
                final line = lines[idx];
                
                // Basit karşılaştırma: Diğer sütunun karşılığıyla karşılaştır
                bool isDifferent = false;
                if (isOld) {
                  isDifferent = _currentFileLines.length <= idx || _currentFileLines[idx] != line;
                } else {
                  isDifferent = _oldVersionLines.length <= idx || _oldVersionLines[idx] != line;
                }

                Color? bg;
                if (isDifferent) {
                  bg = isOld ? Colors.redAccent.withOpacity(0.15) : Colors.greenAccent.withOpacity(0.15);
                }

                return Container(
                  color: bg,
                  padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 2),
                  child: Row(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      SizedBox(
                        width: 28,
                        child: Text(
                          '${idx + 1}',
                          style: TextStyle(fontFamily: 'Consolas', fontSize: 11, color: Colors.grey[700]),
                        ),
                      ),
                      const SizedBox(width: 8),
                      Expanded(
                        child: Text(
                          line,
                          style: TextStyle(
                            fontFamily: 'Consolas',
                            fontSize: 12,
                            color: isDifferent 
                                ? (isOld ? Colors.red[300] : Colors.green[300]) 
                                : Colors.white70,
                          ),
                        ),
                      ),
                    ],
                  ),
                );
              },
            ),
          ),
        )
      ],
    );
  }
}
