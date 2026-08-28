# Privacy Notice

[English](PRIVACY.md) | [简体中文](docs/PRIVACY.zh-CN.md)

Red Packet Assistant is designed to work locally. The upstream project contains no analytics SDK, advertising SDK, account system, remote API client, or telemetry endpoint.

## Data accessed on each platform

### Android

- The notification listener receives notification title/body fields posted by the WeChat package (`com.tencent.mm`) and checks them for a small set of red-packet markers.
- The accessibility service is restricted to the WeChat package and reads system-exposed control text, descriptions, state, and bounds to locate deterministic red-packet actions.
- Local Android preferences store whether automation is enabled, detection/attempt counters, the latest action description, and timestamps.

Notification and accessibility content is matched on-device. The upstream app does not persist message bodies, contact names, chat histories, or the captured accessibility tree.

### macOS

After the user grants Accessibility and Screen Recording permissions, the app reads accessibility labels and control metadata exposed by the running WeChat process. If those labels are insufficient, it captures the visible WeChat window through ScreenCaptureKit and runs Apple's Vision OCR on that image locally in memory. The image is not saved, retained, or uploaded. Matching and counters remain in the app process and are not uploaded.

### Windows

The app reads the UI Automation name and bounds of controls in a visible WeChat window. When a WeChat build exposes its custom-drawn chat canvas as only a few UIA elements, the app captures the visible WeChat window through Windows GDI and evaluates packet-card colors and shapes locally in memory. Captured pixels are not saved, retained, or uploaded. Matching and counters remain in the app process and are not uploaded.

### iOS / iPadOS

The upstream app does not request cross-app data access and does not implement cross-app automation.

## Network use

The application code contains no network client and does not transmit user data. Flutter or platform development tools used to build the project may have their own optional diagnostics; those tools are outside the running application.

## Permissions and control

Automation is disabled by default. Android shows a separate disclosure before linking to notification or accessibility settings. Users can disable the in-app switch or revoke the operating-system permissions at any time.

## Third-party builds

Forks and redistributed builds can change this behavior. Review the exact source and the distributor's privacy policy before installing a third-party build.

## Privacy questions

Do not post notifications, chat screenshots, contact names, or other personal data in a public issue. For a sensitive report, use GitHub's private vulnerability-reporting flow under the repository **Security** tab.
