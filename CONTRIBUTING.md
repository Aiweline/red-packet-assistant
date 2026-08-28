# Contributing

[English](CONTRIBUTING.md) | [简体中文](docs/CONTRIBUTING.zh-CN.md)

Thank you for helping improve Red Packet Assistant.

## Development setup

1. Install Flutter 3.47.1 or a compatible stable release.
2. Run `flutter pub get`.
3. Run `dart analyze lib test` and `flutter test`.
4. Build the platform you changed.

Platform requirements are documented in [README.md](README.md).

## Pull requests

- Keep each pull request focused and explain the user-visible behavior.
- Add the smallest test that reproduces a bug or defines a new requirement.
- Preserve the shared native-channel contract unless the change updates every affected platform.
- Keep all automation off by default and immediately stoppable.
- Update both English and Simplified Chinese documentation for user-visible changes.
- Do not include generated build artifacts, signing material, credentials, notification samples, chat screenshots, or personal data.

## Automation guardrails

Changes must not add credential capture, chat-database access, private APIs, process injection, binary patching, security-control bypasses, deceptive UI actions, remote command execution, or undisclosed data collection.

Android AccessibilityService changes must remain deterministic and narrowly scoped. Any new data access requires an updated prominent disclosure, privacy notice, and tests.

## Validation

Run the checks relevant to your change:

```bash
dart analyze lib test
flutter test
flutter build apk --debug
flutter build macos --debug
flutter build ios --no-codesign
flutter build windows --debug
```

Only commands supported by your host are required locally; CI validates the supported cross-platform matrix.

## Issue reports

Use synthetic text and redact personal information. Security issues belong in GitHub's private vulnerability-reporting flow, not a public issue.
