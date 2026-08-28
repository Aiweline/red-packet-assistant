# 参与贡献

[English](../CONTRIBUTING.md) | [简体中文](CONTRIBUTING.zh-CN.md)

感谢你帮助改进红包助手。

## 开发环境

1. 安装 Flutter 3.47.1 或兼容的稳定版本。
2. 运行 `flutter pub get`。
3. 运行 `dart analyze lib test` 和 `flutter test`。
4. 构建你所修改的平台目标。

各平台环境要求见 [简体中文 README](README.zh-CN.md)。

## Pull Request

- 每个 Pull Request 应聚焦一个目标，并说明用户可见行为。
- 修复缺陷或新增需求时，加入能复现或定义行为的最小测试。
- 除非同步更新所有受影响平台，否则不得破坏共享原生通道协议。
- 自动化必须默认关闭，并可由用户立即停止。
- 用户可见变更须同时更新英文和简体中文文档。
- 不得提交构建产物、签名材料、凭据、真实通知样本、聊天截图或个人数据。

## 自动化边界

变更不得加入账号凭据捕获、聊天数据库访问、私有 API、进程注入、二进制补丁、安全控制绕过、欺骗性界面操作、远程命令执行或未披露的数据收集。

Android AccessibilityService 必须保持规则固定且用途范围有限。任何新增数据访问都必须同步更新醒目披露、隐私说明和测试。

## 验证

运行与你的变更相关的检查：

```bash
dart analyze lib test
flutter test
flutter build apk --debug
flutter build macos --debug
flutter build ios --no-codesign
flutter build windows --debug
```

本地只要求运行当前主机支持的命令；CI 会验证受支持的跨平台矩阵。

## Issue 报告

请使用合成文本并删除个人信息。安全问题应通过 GitHub 私密漏洞报告流程提交，不要发布为公开 Issue。
