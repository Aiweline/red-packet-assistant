import ApplicationServices
import Cocoa
import FlutterMacOS
import ScreenCaptureKit
import Vision

class MainFlutterWindow: NSWindow {
  private var automationBridge: MacAutomationBridge?

  override func awakeFromNib() {
    let flutterViewController = FlutterViewController()
    let windowFrame = self.frame
    self.contentViewController = flutterViewController
    self.setFrame(windowFrame, display: true)

    RegisterGeneratedPlugins(registry: flutterViewController)

    let bridge = MacAutomationBridge(
      messenger: flutterViewController.engine.binaryMessenger
    )
    automationBridge = bridge

    super.awakeFromNib()
  }
}

enum RedPacketOCRPolicy {
  private static let completedMarkers = [
    "已领取",
    "已被领完",
    "已领完",
    "已抢完",
    "已过期",
    "已退款",
    "红包派完了",
    "来晚一步",
  ]

  private static let outcomeMarkers = completedMarkers + [
    "手慢了",
    "查看领取详情",
    "领取详情",
    "领取成功",
    "已存入零钱",
  ]

  private static let popupHeaderMarkers = [
    "发出的红包",
    "给你发了一个红包",
    "给你发的红包",
  ]

  static func normalized(_ label: String) -> String {
    label.components(separatedBy: .whitespacesAndNewlines).joined()
  }

  static func isPacketLabel(_ label: String) -> Bool {
    let value = normalized(label)
    guard !value.isEmpty else { return false }
    let positives = ["微信红包", "领取红包", "查看红包", "发来一个红包"]
    return positives.contains(where: value.contains) && !isCompletedLabel(value)
  }

  static func isOpenLabel(_ label: String) -> Bool {
    let value = normalized(label)
    return value == "开" || value == "拆红包" || value == "领取红包"
  }

  static func fallbackOpenPoint(label: String, box: CGRect) -> CGPoint? {
    let value = normalized(label)
    guard popupHeaderMarkers.contains(where: value.contains),
          box.midX >= 0.2,
          box.midX <= 0.85,
          box.midY >= 0.55,
          box.midY <= 0.92 else {
      return nil
    }
    return CGPoint(
      x: box.midX,
      y: min(max(box.midY - 0.34, 0.32), 0.48)
    )
  }

  static func isPacketPosition(_ box: CGRect) -> Bool {
    box.midX >= 0.25 && box.midX <= 0.97 &&
      box.midY >= 0.08 && box.midY <= 0.96
  }

  static func isCompletedLabel(_ label: String) -> Bool {
    let value = normalized(label)
    return completedMarkers.contains(where: value.contains)
  }

  static func isOutcomeLabel(_ label: String) -> Bool {
    let value = normalized(label)
    return outcomeMarkers.contains(where: value.contains)
  }

  static func isSamePacketCard(
    packetBox: CGRect,
    statusBox: CGRect,
    windowSize: CGSize
  ) -> Bool {
    let verticalDistance =
      (statusBox.midY - packetBox.midY) * windowSize.height
    let horizontalDistance =
      abs(statusBox.midX - packetBox.midX) * windowSize.width
    let horizontalLimit = min(max(windowSize.width * 0.16, 180), 260)
    return verticalDistance >= -8 && verticalDistance <= 110 &&
      horizontalDistance <= horizontalLimit
  }
}

private final class MacAutomationBridge {
  private static let channelName =
    "com.weline.lucky_packet_assistant/automation"
  private static let weChatBundleIdentifiers = [
    "com.tencent.xinWeChat",
    "com.tencent.WeChat",
  ]

  private let channel: FlutterMethodChannel
  private var timer: Timer?
  private var enabled = false
  private var detectedCount = 0
  private var openedCount = 0
  private var lastAction = "暂无记录"
  private var lastActionAt: Int64 = 0
  private var packetStageUntil = Date.distantPast
  private var packetSequence = 0
  private var lastClickAt = Date.distantPast
  private var processedFingerprints = Set<String>()
  private let visionQueue = DispatchQueue(
    label: "com.weline.luckyPacketAssistant.vision",
    qos: .userInitiated
  )
  private var visionScanInFlight = false
  private var visionRetryAfter = Date.distantPast
  private var visionFailureCount = 0

  private struct OCRTarget {
    let label: String
    let kind: String
    let point: CGPoint
    let normalizedY: CGFloat
    let textHeight: CGFloat
  }

  init(messenger: FlutterBinaryMessenger) {
    channel = FlutterMethodChannel(
      name: Self.channelName,
      binaryMessenger: messenger
    )
    channel.setMethodCallHandler { [weak self] call, result in
      self?.handle(call, result: result)
    }
  }

  deinit {
    timer?.invalidate()
  }

  private func handle(_ call: FlutterMethodCall, result: @escaping FlutterResult) {
    switch call.method {
    case "getStatus":
      result(status())
    case "setEnabled":
      guard
        let arguments = call.arguments as? [String: Any],
        let requested = arguments["enabled"] as? Bool
      else {
        result(FlutterError(
          code: "bad_arguments",
          message: "缺少 enabled 参数",
          details: nil
        ))
        return
      }
      if requested && !AXIsProcessTrusted() {
        result(FlutterError(
          code: "accessibility_missing",
          message: "请先授予辅助功能权限",
          details: nil
        ))
        return
      }
      if requested && !CGPreflightScreenCaptureAccess() {
        _ = CGRequestScreenCaptureAccess()
        result(FlutterError(
          code: "screen_capture_missing",
          message: "请先授予屏幕录制权限；识别仅在内存中进行，不保存截图",
          details: nil
        ))
        return
      }
      setEnabled(requested)
      result(nil)
    case "openSettings":
      let target = (call.arguments as? [String: Any])?["target"] as? String
      openPrivacySettings(target: target)
      result(nil)
    case "openWeChat":
      openWeChat(result: result)
    default:
      result(FlutterMethodNotImplemented)
    }
  }

  private func status() -> [String: Any] {
    let trusted = AXIsProcessTrusted()
    let screenCapture = CGPreflightScreenCaptureAccess()
    let running = weChatApplication != nil
    return [
      "platform": "macos",
      "supported": true,
      "enabled": enabled && trusted && screenCapture,
      "notificationAccess": true,
      "accessibilityAccess": trusted,
      "screenCaptureAccess": screenCapture,
      "weChatAvailable": running || installedWeChatURL != nil,
      "statusMessage": statusMessage(
        trusted: trusted,
        screenCapture: screenCapture,
        running: running
      ),
      "lastAction": lastAction,
      "lastActionAt": lastActionAt,
      "detectedCount": detectedCount,
      "openedCount": openedCount,
    ]
  }

  private func statusMessage(
    trusted: Bool,
    screenCapture: Bool,
    running: Bool
  ) -> String {
    if !trusted { return "等待授予 macOS 辅助功能权限" }
    if !screenCapture { return "等待授予 macOS 屏幕录制权限" }
    if !running { return "辅助功能已就绪，请先打开微信" }
    if enabled { return "正在用本地屏幕识别监测微信红包" }
    return "屏幕识别已就绪，自动化当前关闭"
  }

  private func setEnabled(_ requested: Bool) {
    enabled = requested
    if requested {
      lastAction = "助手已启动"
      startPolling()
    } else {
      lastAction = "助手已暂停"
      timer?.invalidate()
      timer = nil
      processedFingerprints.removeAll()
    }
    lastActionAt = Int64(Date().timeIntervalSince1970 * 1000)
  }

  private func startPolling() {
    timer?.invalidate()
    scanWeChat()
    timer = Timer.scheduledTimer(withTimeInterval: 0.25, repeats: true) {
      [weak self] _ in
      self?.scanWeChat()
    }
    if let timer {
      RunLoop.main.add(timer, forMode: .common)
    }
  }

  private var weChatApplication: NSRunningApplication? {
    for identifier in Self.weChatBundleIdentifiers {
      if let application = NSRunningApplication.runningApplications(
        withBundleIdentifier: identifier
      ).first {
        return application
      }
    }
    return nil
  }

  private var installedWeChatURL: URL? {
    for identifier in Self.weChatBundleIdentifiers {
      if let url = NSWorkspace.shared.urlForApplication(
        withBundleIdentifier: identifier
      ) {
        return url
      }
    }
    let fallback = URL(fileURLWithPath: "/Applications/WeChat.app")
    return FileManager.default.fileExists(atPath: fallback.path) ? fallback : nil
  }

  private func openWeChat(result: @escaping FlutterResult) {
    if let application = weChatApplication {
      application.activate(options: [.activateIgnoringOtherApps])
      result(nil)
      return
    }
    guard let url = installedWeChatURL else {
      result(FlutterError(
        code: "wechat_missing",
        message: "未检测到微信",
        details: nil
      ))
      return
    }
    let configuration = NSWorkspace.OpenConfiguration()
    configuration.activates = true
    NSWorkspace.shared.openApplication(at: url, configuration: configuration) {
      _, error in
      DispatchQueue.main.async {
        if let error {
          result(FlutterError(
            code: "wechat_open_failed",
            message: error.localizedDescription,
            details: nil
          ))
        } else {
          result(nil)
        }
      }
    }
  }

  private func openPrivacySettings(target: String?) {
    let pane: String
    if target == "screenCapture" {
      if !CGPreflightScreenCaptureAccess() {
        _ = CGRequestScreenCaptureAccess()
      }
      pane = "Privacy_ScreenCapture"
    } else {
      let options = [
        kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true,
      ] as CFDictionary
      _ = AXIsProcessTrustedWithOptions(options)
      pane = "Privacy_Accessibility"
    }
    let url = URL(
      string: "x-apple.systempreferences:com.apple.preference.security?\(pane)"
    )
    if let url {
      NSWorkspace.shared.open(url)
    }
  }

  private func scanWeChat() {
    guard
      enabled,
      AXIsProcessTrusted(),
      CGPreflightScreenCaptureAccess(),
      let application = weChatApplication
    else {
      return
    }
    let root = AXUIElementCreateApplication(application.processIdentifier)
    var queue: [(AXUIElement, Int)] = [(root, 0)]
    var inspected = 0

    while !queue.isEmpty && inspected < 1800 {
      let (element, depth) = queue.removeFirst()
      inspected += 1
      if depth > 20 { continue }

      let label = searchableLabel(for: element)
      if isPacketLabel(label) {
        if pressOnce(element, label: label, kind: "packet") {
          detectedCount += 1
          openedCount += 1
          packetStageUntil = Date().addingTimeInterval(8)
          record("已点击微信红包控件")
          return
        }
      } else if Date() < packetStageUntil && isOpenLabel(label) {
        if pressOnce(element, label: label, kind: "open") {
          openedCount += 1
          packetStageUntil = Date().addingTimeInterval(8)
          record("已点击“开”红包控件")
          return
        }
      }

      let children = attribute(element, kAXChildrenAttribute) as? [AXUIElement] ?? []
      for child in children.reversed() {
        queue.append((child, depth + 1))
      }
    }

    scanWeChatScreen(application: application)
  }

  private func scanWeChatScreen(application: NSRunningApplication) {
    guard !visionScanInFlight else { return }
    guard Date() >= visionRetryAfter else { return }
    guard #available(macOS 14.0, *) else {
      record("当前 macOS 版本不支持屏幕识别，请升级到 macOS 14 或更高版本")
      return
    }

    visionScanInFlight = true
    let opening = Date() < packetStageUntil
    Task { [weak self] in
      guard let self else { return }
      do {
        let content = try await SCShareableContent.excludingDesktopWindows(
          false,
          onScreenWindowsOnly: false
        )
        let windows = content.windows
          .filter { window in
            guard let identifier = window.owningApplication?.bundleIdentifier else {
              return false
            }
            return Self.weChatBundleIdentifiers.contains(identifier) &&
              window.frame.width >= 220 &&
              window.frame.height >= 180
          }
          .sorted { first, second in
            first.frame.width * first.frame.height >
              second.frame.width * second.frame.height
          }

        let selectedWindows = opening
          ? Array(windows.prefix(2))
          : Array(windows.prefix(1))
        var selectedTarget: OCRTarget?

        for window in selectedWindows {
          let filter = SCContentFilter(desktopIndependentWindow: window)
          let configuration = SCStreamConfiguration()
          configuration.width = max(Int(filter.contentRect.width), 1)
          configuration.height = max(Int(filter.contentRect.height), 1)
          configuration.ignoreShadowsSingleWindow = true
          configuration.scalesToFit = true
          configuration.showsCursor = false

          let image: CGImage
          do {
            image = try await SCScreenshotManager.captureImage(
              contentFilter: filter,
              configuration: configuration
            )
          } catch {
            try await Task.sleep(nanoseconds: 120_000_000)
            let retryConfiguration = SCStreamConfiguration()
            let retryScale = max(CGFloat(filter.pointPixelScale), 1)
            retryConfiguration.width = max(
              Int(filter.contentRect.width * retryScale),
              1
            )
            retryConfiguration.height = max(
              Int(filter.contentRect.height * retryScale),
              1
            )
            retryConfiguration.ignoreShadowsSingleWindow = true
            retryConfiguration.scalesToFit = true
            retryConfiguration.showsCursor = false
            image = try await SCScreenshotManager.captureImage(
              contentFilter: filter,
              configuration: retryConfiguration
            )
          }
          if let target = await recognizeOCRTarget(
            in: image,
            windowFrame: window.frame,
            opening: opening
          ) {
            selectedTarget = target
            break
          }
        }

        let targetToPress = selectedTarget
        await MainActor.run {
          self.visionScanInFlight = false
          self.visionFailureCount = 0
          self.visionRetryAfter = .distantPast
          guard self.enabled else { return }
          if let target = targetToPress {
            self.pressOCRTarget(target, application: application)
          }
        }
      } catch {
        await MainActor.run {
          self.visionScanInFlight = false
          if self.enabled {
            self.visionFailureCount += 1
            let retryDelay = min(
              pow(2, Double(self.visionFailureCount - 1)) * 0.5,
              4
            )
            self.visionRetryAfter = Date().addingTimeInterval(retryDelay)
            if self.visionFailureCount == 1 ||
                self.visionFailureCount.isMultiple(of: 4) {
              self.record(
                "屏幕识别暂时失败，\(String(format: "%.1f", retryDelay)) 秒后重试：" +
                  error.localizedDescription
              )
            }
          }
        }
      }
    }
  }

  private func recognizeOCRTarget(
    in image: CGImage,
    windowFrame: CGRect,
    opening: Bool
  ) async -> OCRTarget? {
    let queue = visionQueue
    return await withCheckedContinuation { continuation in
      queue.async {
        let request = VNRecognizeTextRequest()
        request.recognitionLevel = .accurate
        request.recognitionLanguages = ["zh-Hans"]
        request.usesLanguageCorrection = false
        request.minimumTextHeight = 0.012
        request.customWords = [
          "微信红包",
          "已被领完",
          "已领取",
          "已过期",
          "手慢了，红包派完了",
          "查看领取详情",
          "拆红包",
          "发出的红包",
          "给你发了一个红包",
        ]

        do {
          try VNImageRequestHandler(cgImage: image).perform([request])
        } catch {
          continuation.resume(returning: nil)
          return
        }

        let observations = (request.results ?? []).compactMap { observation ->
          (label: String, box: CGRect, confidence: Float)? in
          guard let candidate = observation.topCandidates(1).first,
                candidate.confidence >= 0.45 else {
            return nil
          }
          return (
            RedPacketOCRPolicy.normalized(candidate.string),
            observation.boundingBox,
            candidate.confidence
          )
        }

        if opening {
          let outcomes = observations.filter { item in
            guard RedPacketOCRPolicy.isOutcomeLabel(item.label),
                  item.box.midX >= 0.2,
                  item.box.midX <= 0.85 else {
              return false
            }
            if item.label.contains("领取详情") {
              return item.box.midY >= 0.12 && item.box.midY <= 0.4
            }
            return item.box.midY >= 0.42 && item.box.midY <= 0.82
          }
          if let outcome = outcomes.max(by: {
            $0.box.height < $1.box.height
          }) {
            let closeY: CGFloat = 0.14
            let point = CGPoint(
              x: windowFrame.minX + outcome.box.midX * windowFrame.width,
              y: windowFrame.minY + (1 - closeY) * windowFrame.height
            )
            continuation.resume(returning: OCRTarget(
              label: outcome.label,
              kind: "close",
              point: point,
              normalizedY: closeY,
              textHeight: outcome.box.height
            ))
            return
          }

          let targets = observations.compactMap { item -> OCRTarget? in
            let label = item.label
            let box = item.box
            guard RedPacketOCRPolicy.isOpenLabel(label),
                  label.count <= 5,
                  box.midX >= 0.2,
                  box.midX <= 0.9,
                  box.midY >= 0.12,
                  box.midY <= 0.9 else {
              return nil
            }
            let point = CGPoint(
              x: windowFrame.minX + box.midX * windowFrame.width,
              y: windowFrame.minY + (1 - box.midY) * windowFrame.height
            )
            return OCRTarget(
              label: label,
              kind: "open",
              point: point,
              normalizedY: box.midY,
              textHeight: box.height
            )
          }
          if let target = targets.max(by: {
            $0.textHeight < $1.textHeight
          }) {
            continuation.resume(returning: target)
            return
          }

          let hasOutcome = observations.contains { item in
            RedPacketOCRPolicy.isOutcomeLabel(item.label)
          }
          if !hasOutcome,
             let fallback = observations.compactMap({ item ->
               (item: (label: String, box: CGRect, confidence: Float), point: CGPoint)? in
               guard let point = RedPacketOCRPolicy.fallbackOpenPoint(
                 label: item.label,
                 box: item.box
               ) else {
                 return nil
               }
               return (item, point)
             }).max(by: { $0.item.confidence < $1.item.confidence }) {
            let point = CGPoint(
              x: windowFrame.minX + fallback.point.x * windowFrame.width,
              y: windowFrame.minY +
                (1 - fallback.point.y) * windowFrame.height
            )
            continuation.resume(returning: OCRTarget(
              label: "红包弹窗：\(fallback.item.label)",
              kind: "open",
              point: point,
              normalizedY: fallback.point.y,
              textHeight: fallback.item.box.height
            ))
            return
          }

          continuation.resume(returning: nil)
          return
        }

        let completedBoxes = observations.compactMap { item in
          RedPacketOCRPolicy.isCompletedLabel(item.label) ? item.box : nil
        }
        let targets = observations.compactMap { item -> OCRTarget? in
          let label = item.label
          let box = item.box
          guard RedPacketOCRPolicy.isPacketLabel(label),
                RedPacketOCRPolicy.isPacketPosition(box) else {
            return nil
          }
          let isCompleted = completedBoxes.contains { statusBox in
            RedPacketOCRPolicy.isSamePacketCard(
              packetBox: box,
              statusBox: statusBox,
              windowSize: windowFrame.size
            )
          }
          guard !isCompleted else { return nil }
          let point = CGPoint(
            x: windowFrame.minX + box.midX * windowFrame.width,
            y: windowFrame.minY + (1 - box.midY) * windowFrame.height
          )
          return OCRTarget(
            label: label,
            kind: "packet",
            point: point,
            normalizedY: box.midY,
            textHeight: box.height
          )
        }
        continuation.resume(returning: targets.min {
          $0.normalizedY < $1.normalizedY
        })
      }
    }
  }

  private func pressOCRTarget(
    _ target: OCRTarget,
    application: NSRunningApplication
  ) {
    guard Date().timeIntervalSince(lastClickAt) > 0.2 else { return }
    let sequence = target.kind == "packet" ? 0 : packetSequence
    let fingerprint = "\(target.kind)|\(sequence)|\(target.label)|" +
      "\(Int(target.point.x)):\(Int(target.point.y))"
    guard !processedFingerprints.contains(fingerprint) else { return }

    lastClickAt = Date()
    let activationDelay = application.isActive ? 0.01 : 0.08
    application.activate(options: [.activateIgnoringOtherApps])
    DispatchQueue.main.asyncAfter(deadline: .now() + activationDelay) { [weak self] in
      guard let self, self.enabled, self.postMouseClick(at: target.point) else {
        return
      }
      self.processedFingerprints.insert(fingerprint)
      if self.processedFingerprints.count > 120 {
        self.processedFingerprints.removeAll(keepingCapacity: true)
        self.processedFingerprints.insert(fingerprint)
      }

      if target.kind == "packet" {
        self.packetSequence += 1
        self.detectedCount += 1
        self.openedCount += 1
        self.packetStageUntil = Date().addingTimeInterval(8)
        self.record("屏幕识别到红包并已点击")
      } else if target.kind == "open" {
        self.openedCount += 1
        self.packetStageUntil = Date().addingTimeInterval(8)
        self.record("屏幕识别到“开”并已点击")
      } else {
        self.packetStageUntil = .distantPast
        self.record("已自动关闭红包结果提示")
      }
    }
  }

  private func postMouseClick(at point: CGPoint) -> Bool {
    guard let source = CGEventSource(stateID: .hidSystemState),
          let down = CGEvent(
            mouseEventSource: source,
            mouseType: .leftMouseDown,
            mouseCursorPosition: point,
            mouseButton: .left
          ),
          let up = CGEvent(
            mouseEventSource: source,
            mouseType: .leftMouseUp,
            mouseCursorPosition: point,
            mouseButton: .left
          ) else {
      return false
    }
    down.post(tap: .cghidEventTap)
    up.post(tap: .cghidEventTap)
    return true
  }

  private func searchableLabel(for element: AXUIElement) -> String {
    let attributes = [
      kAXTitleAttribute,
      kAXValueAttribute,
      kAXDescriptionAttribute,
      kAXHelpAttribute,
    ]
    return attributes.compactMap { attribute(element, $0) as? String }
      .joined(separator: " ")
      .trimmingCharacters(in: .whitespacesAndNewlines)
  }

  private func isPacketLabel(_ label: String) -> Bool {
    RedPacketOCRPolicy.isPacketLabel(label)
  }

  private func isOpenLabel(_ label: String) -> Bool {
    RedPacketOCRPolicy.isOpenLabel(label)
  }

  private func pressOnce(_ element: AXUIElement, label: String, kind: String) -> Bool {
    guard Date().timeIntervalSince(lastClickAt) > 0.2 else { return false }

    var target: AXUIElement? = element
    for _ in 0..<7 {
      guard let current = target else { break }
      let sequence = kind == "open" ? packetSequence : 0
      let fingerprint = "\(kind)|\(sequence)|\(label)|\(elementPosition(current))"
      if !processedFingerprints.contains(fingerprint),
         AXUIElementPerformAction(current, kAXPressAction as CFString) == .success {
        if kind == "packet" { packetSequence += 1 }
        processedFingerprints.insert(fingerprint)
        if processedFingerprints.count > 120 {
          processedFingerprints.removeAll(keepingCapacity: true)
          processedFingerprints.insert(fingerprint)
        }
        lastClickAt = Date()
        return true
      }
      target = parentElement(current)
    }
    return false
  }

  private func elementPosition(_ element: AXUIElement) -> String {
    guard
      let value = attribute(element, kAXPositionAttribute),
      CFGetTypeID(value) == AXValueGetTypeID()
    else {
      return "unknown"
    }
    let raw = unsafeBitCast(value, to: AXValue.self)
    var point = CGPoint.zero
    guard AXValueGetValue(raw, .cgPoint, &point) else { return "unknown" }
    return "\(Int(point.x)):\(Int(point.y))"
  }

  private func parentElement(_ element: AXUIElement) -> AXUIElement? {
    guard
      let value = attribute(element, kAXParentAttribute),
      CFGetTypeID(value) == AXUIElementGetTypeID()
    else {
      return nil
    }
    return unsafeBitCast(value, to: AXUIElement.self)
  }

  private func attribute(
    _ element: AXUIElement,
    _ name: String
  ) -> CFTypeRef? {
    var value: CFTypeRef?
    guard AXUIElementCopyAttributeValue(element, name as CFString, &value) == .success else {
      return nil
    }
    return value
  }

  private func record(_ message: String) {
    lastAction = message
    lastActionAt = Int64(Date().timeIntervalSince1970 * 1000)
  }
}
