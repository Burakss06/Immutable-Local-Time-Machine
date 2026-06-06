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
          background: Color(0xFF090A0F),
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
  
  bool _connected = false;
  bool _isWatching = false;
  List<FileVersion> _history = [];
  String _statusMessage = 'Servise bağlanılmadı.';

  @override
  void initState() {
    super.initState();
    _tryConnect();
  }

  void _tryConnect() {
    setState(() {
      _connected = _bridge.connect();
      _statusMessage = _connected 
          ? 'Servis Bağlantısı Aktif.' 
          : 'Bağlantı hatası! iltm.exe servisinin çalıştığından emin olun.';
    });
  }

  void _disconnect() {
    _bridge.disconnect();
    setState(() {
      _connected = false;
      _history.clear();
      _isWatching = false;
      _statusMessage = 'Bağlantı kesildi.';
    });
  }

  void _queryHistory() {
    if (!_connected) return;
    final path = _pathController.text.trim();
    if (path.isEmpty) return;

    final versions = _bridge.getVersions(path);
    setState(() {
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
    if (_isWatching) {
      final ok = _bridge.stopWatcher();
      if (ok) {
        setState(() {
          _isWatching = false;
          _statusMessage = 'İzleme durduruldu.';
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

  void _restoreVersion(int index) {
    if (!_connected) return;
    final path = _pathController.text.trim();
    // Restoring to the same file path for simplicity
    final ok = _bridge.restore(index, path, path);
    setState(() {
      _statusMessage = ok 
          ? 'Sürüm $index başarıyla geri yüklendi!' 
          : 'Geri yükleme hatası!';
    });
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: Row(
        children: [
          // Left Sidebar (Controls)
          Container(
            width: 340,
            color: const Color(0xFF0F111E),
            padding: const EdgeInsets.all(24.0),
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Row(
                  children: [
                    const Icon(Icons.lock_clock, color: Color(0xFF00FF87), size: 28),
                    const SizedBox(width: 12),
                    Text(
                      'ILTM ENGINE',
                      style: Theme.of(context).textTheme.titleLarge?.copyWith(fontSize: 18),
                    ),
                  ],
                ),
                const SizedBox(width: 1, height: 32),
                
                // Connection Card
                Container(
                  padding: const EdgeInsets.all(16),
                  decoration: BoxDecoration(
                    color: const Color(0xFF181A2A),
                    borderRadius: BorderRadius.circular(12),
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
                                  blurRadius: 8,
                                )
                              ],
                            ),
                          ),
                        ],
                      ),
                      const SizedBox(height: 16),
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
                              child: const Text('Kopart'),
                            ),
                          ),
                        ],
                      )
                    ],
                  ),
                ),
                const SizedBox(height: 32),
                
                // Directory Watcher Panel
                Text('DİZİN GÖZCÜSÜ', style: TextStyle(fontSize: 11, fontWeight: FontWeight.bold, color: Colors.grey[500], letterSpacing: 1)),
                const SizedBox(height: 12),
                TextField(
                  controller: _watchPathController,
                  decoration: const InputDecoration(
                    labelText: 'İzlenecek Klasör',
                    filled: true,
                    fillColor: Color(0xFF181A2A),
                    border: OutlineInputBorder(),
                  ),
                ),
                const SizedBox(height: 12),
                SizedBox(
                  width: double.infinity,
                  child: ElevatedButton(
                    onPressed: _connected ? _toggleWatcher : null,
                    style: ElevatedButton.styleFrom(
                      backgroundColor: _isWatching ? Colors.amber[700] : const Color(0xFF00E5FF),
                      foregroundColor: Colors.black,
                    ),
                    child: Text(_isWatching ? 'Gözcüyü Durdur' : 'İzlemeyi Başlat'),
                  ),
                ),
                const Spacer(),
                
                // Status message footer
                Text(
                  _statusMessage,
                  style: const TextStyle(fontSize: 11, color: Color(0xFF00E5FF)),
                ),
              ],
            ),
          ),
          
          // Right Side (File Timeline)
          Expanded(
            child: Container(
              padding: const EdgeInsets.all(40),
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text('DOSYA SÜRÜM GEÇMİŞİ', style: TextStyle(fontSize: 12, fontWeight: FontWeight.bold, color: Colors.grey[500], letterSpacing: 1.5)),
                  const SizedBox(height: 16),
                  
                  // Query inputs
                  Row(
                    children: [
                      Expanded(
                        child: TextField(
                          controller: _pathController,
                          decoration: const InputDecoration(
                            labelText: 'Sorgulanacak Dosya Yolu',
                            filled: true,
                            fillColor: Color(0xFF0F111E),
                            border: OutlineInputBorder(),
                          ),
                        ),
                      ),
                      const SizedBox(width: 16),
                      SizedBox(
                        height: 56,
                        width: 140,
                        child: ElevatedButton.icon(
                          onPressed: _connected ? _queryHistory : null,
                          icon: const Icon(Icons.search),
                          label: const Text('Sorgula'),
                        ),
                      )
                    ],
                  ),
                  const SizedBox(height: 32),
                  
                  // History list
                  Expanded(
                    child: _history.isEmpty
                        ? Center(
                            child: Text(
                              'Geçmiş kaydı bulunamadı. Lütfen sorgu çalıştırın.',
                              style: TextStyle(color: Colors.grey[600]),
                            ),
                          )
                        : ListView.builder(
                            itemCount: _history.length,
                            itemBuilder: (context, index) {
                              final ver = _history[index];
                              return Card(
                                color: const Color(0xFF0F111E),
                                margin: const EdgeInsets.only(bottom: 12),
                                shape: RoundedRectangleBorder(
                                  borderRadius: BorderRadius.circular(8),
                                  side: const BorderSide(color: Color(0xFF262943)),
                                ),
                                child: ListTile(
                                  leading: const CircleAvatar(
                                    backgroundColor: Color(0xFF1F2235),
                                    child: Icon(Icons.history, color: Color(0xFF00FF87)),
                                  ),
                                  title: Text('Sürüm #${index + 1}'),
                                  subtitle: Text('Zaman Damgası: ${ver.timestamp.toLocal()}\nBlok Adeti: ${ver.blockCount}'),
                                  isThreeLine: true,
                                  trailing: ElevatedButton(
                                    onPressed: () => _restoreVersion(index),
                                    style: ElevatedButton.styleFrom(
                                      backgroundColor: const Color(0xFF262943),
                                    ),
                                    child: const Text('Geri Yükle'),
                                  ),
                                ),
                              );
                            },
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
}
