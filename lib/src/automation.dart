import 'dart:io';

import 'package:flutter/services.dart';

class AutomationStatus {
  const AutomationStatus({
    required this.platform,
    required this.supported,
    required this.enabled,
    required this.notificationAccess,
    required this.accessibilityAccess,
    this.screenCaptureAccess = true,
    required this.weChatAvailable,
    required this.statusMessage,
    required this.lastAction,
    required this.detectedCount,
    required this.openedCount,
    this.lastActionAt,
  });

  factory AutomationStatus.loading() => const AutomationStatus(
    platform: 'unknown',
    supported: true,
    enabled: false,
    notificationAccess: false,
    accessibilityAccess: false,
    screenCaptureAccess: false,
    weChatAvailable: false,
    statusMessage: '正在读取系统状态…',
    lastAction: '暂无记录',
    detectedCount: 0,
    openedCount: 0,
  );

  factory AutomationStatus.fromMap(Map<Object?, Object?> map) {
    int asInt(Object? value) => value is int ? value : 0;
    String asString(Object? value, String fallback) =>
        value is String && value.isNotEmpty ? value : fallback;

    final rawTime = map['lastActionAt'];
    return AutomationStatus(
      platform: asString(map['platform'], 'unknown'),
      supported: map['supported'] == true,
      enabled: map['enabled'] == true,
      notificationAccess: map['notificationAccess'] == true,
      accessibilityAccess: map['accessibilityAccess'] == true,
      screenCaptureAccess: map['screenCaptureAccess'] != false,
      weChatAvailable: map['weChatAvailable'] == true,
      statusMessage: asString(map['statusMessage'], '状态未知'),
      lastAction: asString(map['lastAction'], '暂无记录'),
      detectedCount: asInt(map['detectedCount']),
      openedCount: asInt(map['openedCount']),
      lastActionAt: rawTime is int && rawTime > 0
          ? DateTime.fromMillisecondsSinceEpoch(rawTime)
          : null,
    );
  }

  factory AutomationStatus.unsupportedIOS() => const AutomationStatus(
    platform: 'ios',
    supported: false,
    enabled: false,
    notificationAccess: false,
    accessibilityAccess: false,
    screenCaptureAccess: false,
    weChatAvailable: false,
    statusMessage: 'iOS 不允许第三方应用读取或操作微信界面',
    lastAction: '此设备仅提供使用说明',
    detectedCount: 0,
    openedCount: 0,
  );

  final String platform;
  final bool supported;
  final bool enabled;
  final bool notificationAccess;
  final bool accessibilityAccess;
  final bool screenCaptureAccess;
  final bool weChatAvailable;
  final String statusMessage;
  final String lastAction;
  final int detectedCount;
  final int openedCount;
  final DateTime? lastActionAt;

  bool get readyToEnable {
    switch (platform) {
      case 'android':
        return supported && notificationAccess && accessibilityAccess;
      case 'macos':
        return supported && accessibilityAccess && screenCaptureAccess;
      case 'windows':
        return supported;
      default:
        return false;
    }
  }
}

abstract class AutomationClient {
  Future<AutomationStatus> getStatus();

  Future<void> setEnabled(bool enabled);

  Future<void> openSettings(String target);

  Future<void> openWeChat();
}

class MethodChannelAutomationClient implements AutomationClient {
  static const MethodChannel _channel = MethodChannel(
    'com.weline.lucky_packet_assistant/automation',
  );

  @override
  Future<AutomationStatus> getStatus() async {
    if (Platform.isIOS) {
      return AutomationStatus.unsupportedIOS();
    }

    try {
      final result = await _channel.invokeMapMethod<Object?, Object?>(
        'getStatus',
      );
      return result == null
          ? _fallbackStatus('原生服务未返回状态')
          : AutomationStatus.fromMap(result);
    } on PlatformException catch (error) {
      return _fallbackStatus(error.message ?? '原生服务不可用');
    } on MissingPluginException {
      return _fallbackStatus('此平台的自动化桥接尚未加载');
    }
  }

  @override
  Future<void> openSettings(String target) {
    return _channel.invokeMethod<void>('openSettings', {'target': target});
  }

  @override
  Future<void> openWeChat() {
    return _channel.invokeMethod<void>('openWeChat');
  }

  @override
  Future<void> setEnabled(bool enabled) {
    return _channel.invokeMethod<void>('setEnabled', {'enabled': enabled});
  }

  AutomationStatus _fallbackStatus(String message) => AutomationStatus(
    platform: Platform.operatingSystem,
    supported: false,
    enabled: false,
    notificationAccess: false,
    accessibilityAccess: false,
    screenCaptureAccess: false,
    weChatAvailable: false,
    statusMessage: message,
    lastAction: '暂无记录',
    detectedCount: 0,
    openedCount: 0,
  );
}
