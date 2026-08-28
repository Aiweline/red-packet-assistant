package com.weline.lucky_packet_assistant

import android.content.Intent
import android.provider.Settings
import io.flutter.embedding.android.FlutterActivity
import io.flutter.embedding.engine.FlutterEngine
import io.flutter.plugin.common.MethodChannel

class MainActivity : FlutterActivity() {
    override fun configureFlutterEngine(flutterEngine: FlutterEngine) {
        super.configureFlutterEngine(flutterEngine)
        MethodChannel(
            flutterEngine.dartExecutor.binaryMessenger,
            AutomationState.channelName,
        ).setMethodCallHandler { call, result ->
            when (call.method) {
                "getStatus" -> result.success(AutomationState.status(this))
                "setEnabled" -> {
                    val enabled = call.argument<Boolean>("enabled") ?: false
                    if (enabled && !AutomationState.ready(this)) {
                        result.error(
                            "permissions_missing",
                            "请先开启通知读取和无障碍服务",
                            null,
                        )
                    } else {
                        AutomationState.setEnabled(this, enabled)
                        result.success(null)
                    }
                }
                "openSettings" -> {
                    val target = call.argument<String>("target")
                    val action = if (target == "notification") {
                        Settings.ACTION_NOTIFICATION_LISTENER_SETTINGS
                    } else {
                        Settings.ACTION_ACCESSIBILITY_SETTINGS
                    }
                    startActivity(Intent(action).addFlags(Intent.FLAG_ACTIVITY_NEW_TASK))
                    result.success(null)
                }
                "openWeChat" -> {
                    val intent = packageManager.getLaunchIntentForPackage(
                        AutomationState.weChatPackage,
                    )
                    if (intent == null) {
                        result.error("wechat_missing", "未检测到微信", null)
                    } else {
                        startActivity(intent)
                        result.success(null)
                    }
                }
                else -> result.notImplemented()
            }
        }
    }
}
