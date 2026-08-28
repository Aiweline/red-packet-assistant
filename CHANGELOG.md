# Changelog

All notable project changes are documented here.

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
