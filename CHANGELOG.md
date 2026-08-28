# Changelog

All notable project changes are documented here.

## 1.0.4 - 2026-08-28

- Added an unsigned iOS IPA to tagged GitHub Releases so Android, iOS, macOS, and Windows artifacts are published together.
- Added archive-structure validation for the unsigned IPA before upload.
- Made Simplified Chinese the default repository README and moved the English documentation to `README.en.md`.

## 1.0.3 - 2026-08-28

- Added a Windows GDI visual fallback for WeChat builds that expose the entire custom-drawn chat canvas as only three UI Automation elements.
- Added local orange-card component detection, appearance tracking, and startup baselining so visible old packets are recorded but not clicked.
- Added a packet-stage-only solid-gold open-button detector and automatic Escape closing after the result appears.
- Reduced the Windows polling interval from 650 ms to 250 ms and allowed safe activation of WeChat-owned helper windows.
- Added regression tests for the exact packet color observed in the reported screenshots, faded completed cards, solid open buttons, and outline-only close controls.

## 1.0.2 - 2026-08-28

- Fixed Windows WeChat detection choosing a title-matched helper window instead of the largest visible `WeChat.exe` or `Weixin.exe` window.
- Added Raw UI Automation tree traversal plus Name, HelpText, AutomationId, and Value matching for custom-drawn packet cards.
- Added LegacyIAccessible and process-checked `GetClickablePoint`/`SendInput` activation fallbacks when WeChat does not expose `InvokePattern`.
- Added position guards, completed-card association, second-stage open-button fallback, and separate detected-versus-clicked diagnostics.
- Added Windows-native policy tests to CI and release builds.

## 1.0.1 - 2026-08-28

- Fixed the macOS flow stopping after opening a packet card when Vision could not recognize the stylized circular “开” button.
- Added a geometry fallback that activates only after a known red-packet popup header is recognized.
- Kept result and completed-packet detection ahead of the fallback so finished packets are not reopened.
- Added regression coverage for both the valid popup fallback and ordinary chat-card rejection.
- Changed macOS release packaging to use a stable ad-hoc designated requirement so locally granted permissions can survive compatible package updates.

## 1.0.0 - 2026-08-28

- Added the responsive Flutter dashboard for Android, iOS, macOS, and Windows.
- Added Android notification detection and permission-gated AccessibilityService automation.
- Added explicit Android notification/accessibility disclosures and consent handoffs.
- Added macOS Accessibility-tree detection with a ScreenCaptureKit/Vision OCR fallback, capture retry, action throttling, completed-packet filtering, and automatic outcome-popup closing.
- Added Windows UI Automation traversal and `InvokePattern` actions.
- Added honest iOS platform-boundary messaging without private APIs.
- Added Flutter widget/model tests, Android notification-matcher tests, and macOS native recognition tests.
- Added bilingual documentation, privacy notice, security policy, and contribution guide.
