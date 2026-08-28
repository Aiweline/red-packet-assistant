import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';
import 'package:lucky_packet_assistant/src/app.dart';
import 'package:lucky_packet_assistant/src/automation.dart';

class FakeAutomationClient implements AutomationClient {
  FakeAutomationClient({required this.status});

  AutomationStatus status;
  bool? lastEnabledValue;
  String? lastSettingsTarget;

  @override
  Future<AutomationStatus> getStatus() async => status;

  @override
  Future<void> openSettings(String target) async {
    lastSettingsTarget = target;
  }

  @override
  Future<void> openWeChat() async {}

  @override
  Future<void> setEnabled(bool enabled) async {
    lastEnabledValue = enabled;
    status = AutomationStatus(
      platform: status.platform,
      supported: status.supported,
      enabled: enabled,
      notificationAccess: status.notificationAccess,
      accessibilityAccess: status.accessibilityAccess,
      weChatAvailable: status.weChatAvailable,
      statusMessage: enabled ? '正在监测微信窗口' : '自动化已暂停',
      lastAction: status.lastAction,
      detectedCount: status.detectedCount,
      openedCount: status.openedCount,
    );
  }
}

void main() {
  testWidgets('shows platform state and enables an authorized service', (
    tester,
  ) async {
    final client = FakeAutomationClient(
      status: const AutomationStatus(
        platform: 'macos',
        supported: true,
        enabled: false,
        notificationAccess: true,
        accessibilityAccess: true,
        weChatAvailable: true,
        statusMessage: '辅助功能权限已就绪',
        lastAction: '暂无记录',
        detectedCount: 0,
        openedCount: 0,
      ),
    );

    await tester.pumpWidget(RedPacketApp(automationClient: client));
    await tester.pump();

    expect(find.text('红包助手'), findsOneWidget);
    expect(find.text('macOS'), findsOneWidget);
    expect(find.text('准备就绪'), findsOneWidget);

    await tester.tap(find.byType(Switch));
    await tester.pump();

    expect(client.lastEnabledValue, isTrue);
    expect(find.text('等待红包出现'), findsOneWidget);
  });

  test('parses a native status payload', () {
    final status = AutomationStatus.fromMap({
      'platform': 'android',
      'supported': true,
      'enabled': true,
      'notificationAccess': true,
      'accessibilityAccess': true,
      'weChatAvailable': true,
      'statusMessage': '运行中',
      'lastAction': '发现红包',
      'detectedCount': 2,
      'openedCount': 1,
      'lastActionAt': 1234,
    });

    expect(status.readyToEnable, isTrue);
    expect(status.detectedCount, 2);
    expect(status.lastActionAt?.millisecondsSinceEpoch, 1234);
  });

  test('requires macOS screen capture before automation can start', () {
    final missingCapture = AutomationStatus.fromMap(const {
      'platform': 'macos',
      'supported': true,
      'accessibilityAccess': true,
      'screenCaptureAccess': false,
    });
    final ready = AutomationStatus.fromMap(const {
      'platform': 'macos',
      'supported': true,
      'accessibilityAccess': true,
      'screenCaptureAccess': true,
    });

    expect(missingCapture.readyToEnable, isFalse);
    expect(ready.readyToEnable, isTrue);
  });

  testWidgets('shows the honest iOS capability boundary', (tester) async {
    final client = FakeAutomationClient(
      status: AutomationStatus.unsupportedIOS(),
    );

    await tester.pumpWidget(RedPacketApp(automationClient: client));
    await tester.pump();

    expect(find.text('iOS 系统限制'), findsOneWidget);
    expect(find.text('受系统限制'), findsOneWidget);
    expect(find.byType(Switch), findsOneWidget);
    expect(tester.widget<Switch>(find.byType(Switch)).onChanged, isNull);
  });

  test('treats a zero native timestamp as no activity', () {
    final status = AutomationStatus.fromMap(const {
      'platform': 'macos',
      'supported': true,
      'lastActionAt': 0,
    });

    expect(status.lastActionAt, isNull);
  });

  testWidgets('requires Android disclosure consent before opening settings', (
    tester,
  ) async {
    final client = FakeAutomationClient(
      status: const AutomationStatus(
        platform: 'android',
        supported: true,
        enabled: false,
        notificationAccess: false,
        accessibilityAccess: false,
        weChatAvailable: true,
        statusMessage: '等待授权',
        lastAction: '暂无记录',
        detectedCount: 0,
        openedCount: 0,
      ),
    );

    await tester.pumpWidget(RedPacketApp(automationClient: client));
    await tester.pump();
    await tester.tap(find.text('通知读取'));
    await tester.pumpAndSettle();

    expect(find.text('通知读取使用说明'), findsOneWidget);
    expect(client.lastSettingsTarget, isNull);

    await tester.tap(find.text('同意并前往设置'));
    await tester.pumpAndSettle();

    expect(client.lastSettingsTarget, 'notification');
  });
}
