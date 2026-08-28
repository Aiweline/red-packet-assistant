# Security Policy

[English](SECURITY.md) | [简体中文](docs/SECURITY.zh-CN.md)

## Reporting a vulnerability

Please use GitHub's private vulnerability-reporting flow under the repository **Security** tab. Do not disclose a vulnerability publicly before a fix is available, and never include real chat content, notification payloads, credentials, contact data, or signing keys in a report.

Include:

- affected platform and OS version;
- app commit or release version;
- WeChat version;
- a minimal reproduction using synthetic data;
- expected and observed behavior;
- your assessment of user impact.

## Security boundaries

The upstream project must not:

- request or store WeChat credentials;
- access the WeChat chat database;
- upload notification or accessibility content;
- inject code into WeChat or patch its binaries;
- bypass OS security controls, private-API restrictions, jailbreak protections, or permission prompts;
- include keystores, certificates, tokens, cookies, or other credentials in source control.

Automation must remain disabled by default, permission-gated, deterministic, narrowly scoped to red-packet markers, and immediately stoppable by the user.

## Supported source

Security fixes target the latest default branch. Binary files from unofficial forks or third-party distributors are outside the upstream project's trust boundary.
