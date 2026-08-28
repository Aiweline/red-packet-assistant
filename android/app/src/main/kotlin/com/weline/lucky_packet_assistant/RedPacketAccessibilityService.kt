package com.weline.lucky_packet_assistant

import android.accessibilityservice.AccessibilityService
import android.graphics.Rect
import android.os.Handler
import android.os.Looper
import android.view.accessibility.AccessibilityEvent
import android.view.accessibility.AccessibilityNodeInfo

class RedPacketAccessibilityService : AccessibilityService() {
    private val handler = Handler(Looper.getMainLooper())
    private var lastClickAt = 0L
    private val clickedFingerprints = mutableMapOf<String, Long>()

    override fun onAccessibilityEvent(event: AccessibilityEvent?) {
        if (!AutomationState.isEnabled(this)) return
        if (event?.packageName?.toString() != AutomationState.weChatPackage) return
        if (event.eventType != AccessibilityEvent.TYPE_WINDOW_CONTENT_CHANGED &&
            event.eventType != AccessibilityEvent.TYPE_WINDOW_STATE_CHANGED
        ) {
            return
        }
        handler.removeCallbacksAndMessages(null)
        handler.postDelayed({ clickActionableNode() }, 220)
    }

    override fun onInterrupt() {
        handler.removeCallbacksAndMessages(null)
    }

    private fun clickActionableNode() {
        if (System.currentTimeMillis() - lastClickAt < 850) return
        val root = rootInActiveWindow ?: return

        val packetCandidate = findFirst(
            root,
            listOf("领取红包", "微信红包", "查看红包"),
        )
        val candidate = packetCandidate ?: if (AutomationState.hasRecentPacket(this)) {
            findExact(root, "开") ?: return
        } else {
            return
        }
        if (containsTerminalText(candidate)) return

        val clickable = clickableAncestor(candidate) ?: return
        val now = System.currentTimeMillis()
        clickedFingerprints.entries.removeAll { now - it.value > 45_000 }
        val fingerprint = fingerprint(candidate)
        if (clickedFingerprints.containsKey(fingerprint)) return
        if (clickable.performAction(AccessibilityNodeInfo.ACTION_CLICK)) {
            lastClickAt = now
            clickedFingerprints[fingerprint] = now
            if (packetCandidate != null && !AutomationState.hasRecentPacket(this)) {
                AutomationState.recordDetected(this, "在微信界面发现红包")
            }
            AutomationState.recordOpened(this, "已点击可访问的红包控件")
            handler.postDelayed({ clickActionableNode() }, 650)
        }
    }

    private fun findFirst(
        root: AccessibilityNodeInfo,
        keywords: List<String>,
    ): AccessibilityNodeInfo? {
        for (keyword in keywords) {
            val matches = root.findAccessibilityNodeInfosByText(keyword)
            val candidate = matches.firstOrNull {
                !containsTerminalText(it) && clickableAncestor(it) != null
            }
            if (candidate != null) return candidate
        }
        return null
    }

    private fun findExact(
        node: AccessibilityNodeInfo,
        expected: String,
        depth: Int = 0,
    ): AccessibilityNodeInfo? {
        if (depth > 18) return null
        val text = node.text?.toString()?.trim()
        val description = node.contentDescription?.toString()?.trim()
        if ((text == expected || description == expected) &&
            clickableAncestor(node) != null
        ) {
            return node
        }
        for (index in 0 until node.childCount) {
            val child = node.getChild(index) ?: continue
            findExact(child, expected, depth + 1)?.let { return it }
        }
        return null
    }

    private fun clickableAncestor(node: AccessibilityNodeInfo): AccessibilityNodeInfo? {
        var current: AccessibilityNodeInfo? = node
        repeat(6) {
            if (current?.isClickable == true && current?.isEnabled == true) return current
            current = current?.parent
        }
        return null
    }

    private fun containsTerminalText(node: AccessibilityNodeInfo): Boolean {
        val text = listOf(node.text, node.contentDescription)
            .filterNotNull()
            .joinToString(" ")
        return listOf("已领取", "已被领完", "已领完", "已过期").any(text::contains)
    }

    private fun fingerprint(node: AccessibilityNodeInfo): String {
        val bounds = Rect()
        node.getBoundsInScreen(bounds)
        return listOf(
            node.windowId,
            node.viewIdResourceName.orEmpty(),
            node.text?.toString().orEmpty(),
            node.contentDescription?.toString().orEmpty(),
            bounds.flattenToString(),
        ).joinToString("|")
    }
}
