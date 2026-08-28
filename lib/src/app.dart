import 'dart:async';

import 'package:flutter/material.dart';

import 'automation.dart';

const Color _red = Color(0xFFD92D20);
const Color _darkRed = Color(0xFF8E1B13);
const Color _gold = Color(0xFFF5B942);
const Color _ink = Color(0xFF2D201D);
const Color _paper = Color(0xFFFFF8F2);

class RedPacketApp extends StatelessWidget {
  const RedPacketApp({super.key, this.automationClient});

  final AutomationClient? automationClient;

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      debugShowCheckedModeBanner: false,
      title: '红包助手',
      theme: ThemeData(
        useMaterial3: true,
        colorScheme: ColorScheme.fromSeed(
          seedColor: _red,
          brightness: Brightness.light,
          surface: _paper,
        ),
        scaffoldBackgroundColor: const Color(0xFFFFFCF9),
        cardTheme: const CardThemeData(
          elevation: 0,
          margin: EdgeInsets.zero,
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.all(Radius.circular(22)),
            side: BorderSide(color: Color(0xFFF0E2D8)),
          ),
        ),
        textTheme: const TextTheme(
          headlineMedium: TextStyle(
            color: _ink,
            fontWeight: FontWeight.w800,
            letterSpacing: -0.6,
          ),
          titleLarge: TextStyle(color: _ink, fontWeight: FontWeight.w700),
          titleMedium: TextStyle(color: _ink, fontWeight: FontWeight.w700),
          bodyLarge: TextStyle(color: _ink, height: 1.45),
          bodyMedium: TextStyle(color: Color(0xFF695A55), height: 1.45),
        ),
      ),
      home: DashboardPage(
        automationClient: automationClient ?? MethodChannelAutomationClient(),
      ),
    );
  }
}

class DashboardPage extends StatefulWidget {
  const DashboardPage({super.key, required this.automationClient});

  final AutomationClient automationClient;

  @override
  State<DashboardPage> createState() => _DashboardPageState();
}

class _DashboardPageState extends State<DashboardPage> {
  AutomationStatus _status = AutomationStatus.loading();
  Timer? _pollTimer;
  bool _busy = false;

  @override
  void initState() {
    super.initState();
    _refresh();
    _pollTimer = Timer.periodic(
      const Duration(seconds: 2),
      (_) => _refresh(silent: true),
    );
  }

  @override
  void dispose() {
    _pollTimer?.cancel();
    super.dispose();
  }

  Future<void> _refresh({bool silent = false}) async {
    try {
      final next = await widget.automationClient.getStatus();
      if (mounted) {
        setState(() => _status = next);
      }
    } catch (error) {
      if (!silent && mounted) {
        _showMessage('读取状态失败：$error');
      }
    }
  }

  Future<void> _toggle(bool enabled) async {
    if (_busy) return;
    if (enabled && !_status.readyToEnable) {
      _showMessage(_permissionHint);
      return;
    }

    setState(() => _busy = true);
    try {
      await widget.automationClient.setEnabled(enabled);
      await _refresh();
    } catch (error) {
      if (mounted) _showMessage('切换失败：$error');
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  Future<void> _openSettings(String target) async {
    try {
      await widget.automationClient.openSettings(target);
    } catch (error) {
      if (mounted) _showMessage('无法打开系统设置：$error');
    }
  }

  Future<void> _requestAndroidPermission(String target) async {
    final accessibility = target == 'accessibility';
    final accepted = await showDialog<bool>(
      context: context,
      builder: (context) => AlertDialog(
        icon: Icon(
          accessibility
              ? Icons.accessibility_new_rounded
              : Icons.notifications_active_outlined,
          color: _red,
        ),
        title: Text(accessibility ? '无障碍服务使用说明' : '通知读取使用说明'),
        content: Text(
          accessibility
              ? '红包助手会读取微信界面向 Android 系统公开的控件文字和状态，只用于识别并点击“微信红包”“领取红包”和“开”等控件。开启后，助手可代表你执行这些点击。数据仅在本机即时处理，不保存、不上传；你可以随时关闭服务。'
              : '红包助手会读取微信通知的标题和正文，只用于判断是否收到可领取的红包并打开对应会话。通知内容仅在本机即时匹配，不保存、不上传；你可以随时撤销通知读取权限。',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(context).pop(false),
            child: const Text('暂不授权'),
          ),
          FilledButton(
            onPressed: () => Navigator.of(context).pop(true),
            child: const Text('同意并前往设置'),
          ),
        ],
      ),
    );
    if (accepted == true) {
      await _openSettings(target);
    }
  }

  Future<void> _openWeChat() async {
    try {
      await widget.automationClient.openWeChat();
    } catch (error) {
      if (mounted) _showMessage('未能打开微信：$error');
    }
  }

  void _showMessage(String message) {
    ScaffoldMessenger.of(context)
      ..hideCurrentSnackBar()
      ..showSnackBar(SnackBar(content: Text(message)));
  }

  String get _platformName {
    switch (_status.platform) {
      case 'android':
        return 'Android';
      case 'ios':
        return 'iPhone / iPad';
      case 'macos':
        return 'macOS';
      case 'windows':
        return 'Windows';
      default:
        return '当前设备';
    }
  }

  String get _permissionHint {
    switch (_status.platform) {
      case 'android':
        return '请先授予通知读取和无障碍两项权限';
      case 'macos':
        return '请先授予辅助功能与屏幕录制两项权限';
      case 'ios':
        return 'iOS 不允许第三方应用自动读取或点击微信';
      default:
        return '当前平台尚未满足运行条件';
    }
  }

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      body: SafeArea(
        child: CustomScrollView(
          slivers: [
            SliverToBoxAdapter(child: _buildHeader()),
            SliverPadding(
              padding: const EdgeInsets.fromLTRB(20, 8, 20, 32),
              sliver: SliverToBoxAdapter(
                child: Center(
                  child: ConstrainedBox(
                    constraints: const BoxConstraints(maxWidth: 1080),
                    child: LayoutBuilder(
                      builder: (context, constraints) {
                        final wide = constraints.maxWidth >= 760;
                        final main = _buildMainCard();
                        final permissions = _buildPermissionsCard();
                        return Column(
                          children: [
                            if (wide)
                              Row(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Expanded(flex: 6, child: main),
                                  const SizedBox(width: 18),
                                  Expanded(flex: 5, child: permissions),
                                ],
                              )
                            else ...[
                              main,
                              const SizedBox(height: 16),
                              permissions,
                            ],
                            const SizedBox(height: 18),
                            _buildActivityCard(),
                            const SizedBox(height: 18),
                            _buildBoundaryCard(),
                          ],
                        );
                      },
                    ),
                  ),
                ),
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildHeader() {
    return Center(
      child: ConstrainedBox(
        constraints: const BoxConstraints(maxWidth: 1120),
        child: Padding(
          padding: const EdgeInsets.fromLTRB(24, 24, 24, 20),
          child: Row(
            children: [
              Container(
                width: 46,
                height: 46,
                decoration: BoxDecoration(
                  borderRadius: BorderRadius.circular(14),
                  boxShadow: const [
                    BoxShadow(
                      color: Color(0x33D92D20),
                      blurRadius: 18,
                      offset: Offset(0, 7),
                    ),
                  ],
                ),
                clipBehavior: Clip.antiAlias,
                child: Image.asset(
                  'assets/branding/app-icon-256.png',
                  fit: BoxFit.cover,
                  semanticLabel: '红包助手图标',
                ),
              ),
              const SizedBox(width: 14),
              const Expanded(
                child: Column(
                  crossAxisAlignment: CrossAxisAlignment.start,
                  children: [
                    Text(
                      '红包助手',
                      style: TextStyle(
                        color: _ink,
                        fontSize: 22,
                        fontWeight: FontWeight.w800,
                      ),
                    ),
                    Text(
                      '本地识别 · 不保存聊天内容 · 不上传数据',
                      style: TextStyle(color: Color(0xFF83716A), fontSize: 12),
                    ),
                  ],
                ),
              ),
              Chip(
                avatar: const Icon(Icons.devices_rounded, size: 16),
                label: Text(_platformName),
                side: BorderSide.none,
                backgroundColor: const Color(0xFFF8ECE4),
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _buildMainCard() {
    final active = _status.enabled;
    final available = _status.supported;
    return Container(
      decoration: BoxDecoration(
        gradient: LinearGradient(
          colors: active
              ? const [_red, _darkRed]
              : const [Color(0xFF4D403C), Color(0xFF2D2522)],
          begin: Alignment.topLeft,
          end: Alignment.bottomRight,
        ),
        borderRadius: BorderRadius.circular(26),
        boxShadow: const [
          BoxShadow(
            color: Color(0x1F6E3128),
            blurRadius: 30,
            offset: Offset(0, 14),
          ),
        ],
      ),
      child: Padding(
        padding: const EdgeInsets.all(26),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Container(
                  width: 12,
                  height: 12,
                  decoration: BoxDecoration(
                    color: active ? const Color(0xFF7CF4A5) : Colors.white38,
                    shape: BoxShape.circle,
                  ),
                ),
                const SizedBox(width: 9),
                Text(
                  active ? '助手正在运行' : '助手已暂停',
                  style: const TextStyle(
                    color: Colors.white,
                    fontSize: 15,
                    fontWeight: FontWeight.w700,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 22),
            Text(
              available ? (active ? '等待红包出现' : '准备就绪') : '受系统限制',
              style: const TextStyle(
                color: Colors.white,
                fontSize: 30,
                fontWeight: FontWeight.w800,
                letterSpacing: -0.8,
              ),
            ),
            const SizedBox(height: 8),
            Text(
              _status.statusMessage,
              style: const TextStyle(color: Colors.white70, height: 1.45),
            ),
            const SizedBox(height: 24),
            Container(
              padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 8),
              decoration: BoxDecoration(
                color: Colors.white.withValues(alpha: 0.12),
                borderRadius: BorderRadius.circular(16),
              ),
              child: Row(
                children: [
                  const Expanded(
                    child: Text(
                      '自动处理红包',
                      style: TextStyle(
                        color: Colors.white,
                        fontWeight: FontWeight.w600,
                      ),
                    ),
                  ),
                  Switch(
                    value: active,
                    onChanged: available && !_busy ? _toggle : null,
                    activeThumbColor: _gold,
                    activeTrackColor: Colors.white24,
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildPermissionsCard() {
    final rows = <Widget>[];
    if (_status.platform == 'android') {
      rows.addAll([
        _permissionRow(
          icon: Icons.notifications_active_outlined,
          title: '通知读取',
          granted: _status.notificationAccess,
          onTap: () => _requestAndroidPermission('notification'),
        ),
        const Divider(height: 26),
        _permissionRow(
          icon: Icons.touch_app_outlined,
          title: '无障碍服务',
          granted: _status.accessibilityAccess,
          onTap: () => _requestAndroidPermission('accessibility'),
        ),
      ]);
    } else if (_status.platform == 'macos') {
      rows.addAll([
        _permissionRow(
          icon: Icons.accessibility_new_rounded,
          title: '辅助功能权限',
          granted: _status.accessibilityAccess,
          onTap: () => _openSettings('accessibility'),
        ),
        const Divider(height: 26),
        _permissionRow(
          icon: Icons.monitor_outlined,
          title: '屏幕录制权限',
          granted: _status.screenCaptureAccess,
          onTap: () => _openSettings('screenCapture'),
        ),
      ]);
    } else if (_status.platform == 'windows') {
      rows.add(
        _permissionRow(
          icon: Icons.desktop_windows_outlined,
          title: 'Windows UI Automation',
          granted: _status.accessibilityAccess,
          onTap: () => _openSettings('accessibility'),
        ),
      );
    } else {
      rows.add(
        const _InfoRow(
          icon: Icons.lock_outline_rounded,
          title: 'iOS 系统限制',
          subtitle: '第三方应用无法读取微信通知，也无法代替用户点击。',
        ),
      );
    }

    return Card(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                const Expanded(
                  child: Text(
                    '系统授权',
                    style: TextStyle(fontSize: 18, fontWeight: FontWeight.w800),
                  ),
                ),
                IconButton(
                  tooltip: '刷新状态',
                  onPressed: _refresh,
                  icon: const Icon(Icons.refresh_rounded),
                ),
              ],
            ),
            const SizedBox(height: 14),
            ...rows,
            if (_status.platform != 'ios') ...[
              const SizedBox(height: 20),
              SizedBox(
                width: double.infinity,
                child: OutlinedButton.icon(
                  onPressed: _openWeChat,
                  icon: const Icon(Icons.open_in_new_rounded, size: 18),
                  label: const Text('打开微信'),
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }

  Widget _permissionRow({
    required IconData icon,
    required String title,
    required bool granted,
    required VoidCallback onTap,
  }) {
    return InkWell(
      borderRadius: BorderRadius.circular(14),
      onTap: granted ? null : onTap,
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 5),
        child: Row(
          children: [
            Container(
              width: 42,
              height: 42,
              decoration: BoxDecoration(
                color: granted
                    ? const Color(0xFFE9F8EE)
                    : const Color(0xFFFFF0E5),
                borderRadius: BorderRadius.circular(12),
              ),
              child: Icon(
                icon,
                color: granted ? const Color(0xFF208A46) : _red,
              ),
            ),
            const SizedBox(width: 13),
            Expanded(
              child: Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(
                    title,
                    style: const TextStyle(fontWeight: FontWeight.w700),
                  ),
                  Text(
                    granted ? '已授权' : '未授权 · 点击前往设置',
                    style: TextStyle(
                      color: granted
                          ? const Color(0xFF208A46)
                          : const Color(0xFF8A6254),
                      fontSize: 12,
                    ),
                  ),
                ],
              ),
            ),
            Icon(
              granted
                  ? Icons.check_circle_rounded
                  : Icons.chevron_right_rounded,
              color: granted ? const Color(0xFF2DAA5C) : Colors.black38,
            ),
          ],
        ),
      ),
    );
  }

  Widget _buildActivityCard() {
    return Card(
      child: Padding(
        padding: const EdgeInsets.all(24),
        child: Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            const Text(
              '运行记录',
              style: TextStyle(fontSize: 18, fontWeight: FontWeight.w800),
            ),
            const SizedBox(height: 18),
            Row(
              children: [
                Expanded(
                  child: _metric(
                    '发现红包',
                    _status.detectedCount,
                    Icons.search_rounded,
                  ),
                ),
                Container(width: 1, height: 46, color: const Color(0xFFEEDFD5)),
                Expanded(
                  child: _metric(
                    '已尝试打开',
                    _status.openedCount,
                    Icons.bolt_rounded,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 20),
            Container(
              width: double.infinity,
              padding: const EdgeInsets.all(16),
              decoration: BoxDecoration(
                color: const Color(0xFFFAF3EE),
                borderRadius: BorderRadius.circular(15),
              ),
              child: Row(
                children: [
                  const Icon(Icons.history_rounded, color: Color(0xFF8A6E64)),
                  const SizedBox(width: 12),
                  Expanded(
                    child: Column(
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text(
                          _status.lastAction,
                          style: const TextStyle(fontWeight: FontWeight.w600),
                        ),
                        Text(
                          _formatTime(_status.lastActionAt),
                          style: const TextStyle(
                            color: Color(0xFF8A7770),
                            fontSize: 12,
                          ),
                        ),
                      ],
                    ),
                  ),
                ],
              ),
            ),
          ],
        ),
      ),
    );
  }

  Widget _metric(String label, int value, IconData icon) {
    return Row(
      mainAxisAlignment: MainAxisAlignment.center,
      children: [
        Icon(icon, color: _red, size: 22),
        const SizedBox(width: 10),
        Column(
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(
              '$value',
              style: const TextStyle(
                fontSize: 24,
                fontWeight: FontWeight.w800,
                color: _ink,
              ),
            ),
            Text(
              label,
              style: const TextStyle(fontSize: 12, color: Color(0xFF87746D)),
            ),
          ],
        ),
      ],
    );
  }

  Widget _buildBoundaryCard() {
    final platformDetail = switch (_status.platform) {
      'android' => '依赖微信通知文本和页面可访问性；微信版本变化可能导致识别失效。',
      'macos' =>
        '微信未公开聊天控件时，使用 macOS 屏幕录制与 Vision 在内存中识别红包文字，再通过辅助功能执行点击；截图不保存、不上传。',
      'windows' => '依赖 Windows UI Automation；微信控件结构更新后可能需要适配。',
      'ios' => '这里只提供说明页。越狱、私有 API 或绕过系统保护均不在实现范围内。',
      _ => '当前平台没有可用的自动化实现。',
    };
    return Container(
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: const Color(0xFFFFF7DF),
        borderRadius: BorderRadius.circular(18),
        border: Border.all(color: const Color(0xFFF2D98B)),
      ),
      child: Row(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Icon(Icons.shield_outlined, color: Color(0xFF9A6A00)),
          const SizedBox(width: 13),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                const Text(
                  '能力边界',
                  style: TextStyle(
                    fontWeight: FontWeight.w800,
                    color: Color(0xFF6F4B00),
                  ),
                ),
                const SizedBox(height: 5),
                Text(
                  '$platformDetail 请仅在自己的设备和账号上使用，并自行确认微信及应用商店规则。',
                  style: const TextStyle(
                    color: Color(0xFF765E28),
                    height: 1.45,
                  ),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }

  String _formatTime(DateTime? time) {
    if (time == null) return '尚未产生自动化记录';
    String two(int value) => value.toString().padLeft(2, '0');
    return '${time.month}月${time.day}日 '
        '${two(time.hour)}:${two(time.minute)}:${two(time.second)}';
  }
}

class _InfoRow extends StatelessWidget {
  const _InfoRow({
    required this.icon,
    required this.title,
    required this.subtitle,
  });

  final IconData icon;
  final String title;
  final String subtitle;

  @override
  Widget build(BuildContext context) {
    return Row(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Container(
          width: 42,
          height: 42,
          decoration: BoxDecoration(
            color: const Color(0xFFF2ECE8),
            borderRadius: BorderRadius.circular(12),
          ),
          child: Icon(icon, color: const Color(0xFF71635E)),
        ),
        const SizedBox(width: 13),
        Expanded(
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(title, style: const TextStyle(fontWeight: FontWeight.w700)),
              const SizedBox(height: 3),
              Text(subtitle, style: Theme.of(context).textTheme.bodySmall),
            ],
          ),
        ),
      ],
    );
  }
}
