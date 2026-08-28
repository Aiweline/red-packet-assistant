package com.weline.lucky_packet_assistant

import android.content.ComponentName
import android.content.Context
import android.content.pm.PackageManager
import android.provider.Settings

object AutomationState {
    const val channelName = "com.weline.lucky_packet_assistant/automation"
    const val weChatPackage = "com.tencent.mm"

    private const val preferencesName = "red_packet_automation"
    private const val enabledKey = "enabled"
    private const val detectedCountKey = "detected_count"
    private const val openedCountKey = "opened_count"
    private const val lastActionKey = "last_action"
    private const val lastActionAtKey = "last_action_at"
    private const val pendingAtKey = "pending_at"

    fun status(context: Context): Map<String, Any> {
        val preferences = preferences(context)
        val notificationAccess = hasNotificationAccess(context)
        val accessibilityAccess = hasAccessibilityAccess(context)
        val enabled = preferences.getBoolean(enabledKey, false) &&
            notificationAccess && accessibilityAccess
        return mapOf(
            "platform" to "android",
            "supported" to true,
            "enabled" to enabled,
            "notificationAccess" to notificationAccess,
            "accessibilityAccess" to accessibilityAccess,
            "weChatAvailable" to isWeChatInstalled(context),
            "statusMessage" to when {
                !notificationAccess -> "等待开启通知读取权限"
                !accessibilityAccess -> "等待开启无障碍服务"
                enabled -> "监听微信红包通知，并尝试点击可访问的红包控件"
                else -> "两项权限已就绪，自动化当前关闭"
            },
            "lastAction" to preferences.getString(lastActionKey, "暂无记录").orEmpty(),
            "lastActionAt" to preferences.getLong(lastActionAtKey, 0L),
            "detectedCount" to preferences.getInt(detectedCountKey, 0),
            "openedCount" to preferences.getInt(openedCountKey, 0),
        )
    }

    fun ready(context: Context): Boolean =
        hasNotificationAccess(context) && hasAccessibilityAccess(context)

    fun isEnabled(context: Context): Boolean =
        preferences(context).getBoolean(enabledKey, false)

    fun setEnabled(context: Context, enabled: Boolean) {
        preferences(context).edit()
            .putBoolean(enabledKey, enabled)
            .putString(lastActionKey, if (enabled) "助手已启动" else "助手已暂停")
            .putLong(lastActionAtKey, System.currentTimeMillis())
            .apply()
    }

    fun recordDetected(context: Context, message: String) {
        val preferences = preferences(context)
        preferences.edit()
            .putInt(detectedCountKey, preferences.getInt(detectedCountKey, 0) + 1)
            .putString(lastActionKey, message)
            .putLong(lastActionAtKey, System.currentTimeMillis())
            .putLong(pendingAtKey, System.currentTimeMillis())
            .apply()
    }

    fun recordOpened(context: Context, message: String) {
        val preferences = preferences(context)
        preferences.edit()
            .putInt(openedCountKey, preferences.getInt(openedCountKey, 0) + 1)
            .putString(lastActionKey, message)
            .putLong(lastActionAtKey, System.currentTimeMillis())
            .apply()
    }

    fun hasRecentPacket(context: Context): Boolean {
        val pendingAt = preferences(context).getLong(pendingAtKey, 0L)
        return System.currentTimeMillis() - pendingAt in 0..20_000
    }

    private fun hasNotificationAccess(context: Context): Boolean {
        val enabled = Settings.Secure.getString(
            context.contentResolver,
            "enabled_notification_listeners",
        ).orEmpty()
        val component = ComponentName(
            context,
            WeChatNotificationListenerService::class.java,
        )
        return enabled.split(':').any {
            ComponentName.unflattenFromString(it) == component
        }
    }

    private fun hasAccessibilityAccess(context: Context): Boolean {
        val enabled = Settings.Secure.getString(
            context.contentResolver,
            Settings.Secure.ENABLED_ACCESSIBILITY_SERVICES,
        ).orEmpty()
        val component = ComponentName(
            context,
            RedPacketAccessibilityService::class.java,
        )
        return enabled.split(':').any {
            ComponentName.unflattenFromString(it) == component
        }
    }

    private fun isWeChatInstalled(context: Context): Boolean = try {
        context.packageManager.getPackageInfo(weChatPackage, 0)
        true
    } catch (_: PackageManager.NameNotFoundException) {
        false
    }

    private fun preferences(context: Context) =
        context.getSharedPreferences(preferencesName, Context.MODE_PRIVATE)
}
