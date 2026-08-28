#!/bin/bash

set -euo pipefail

app_path="${1:-build/macos/Build/Products/Release/红包助手.app}"

if [[ ! -d "$app_path" ]]; then
  echo "macOS app bundle not found: $app_path" >&2
  exit 1
fi

codesign --force --sign - \
  "$app_path/Contents/Frameworks/FlutterMacOS.framework"
codesign --force --sign - \
  "$app_path/Contents/Frameworks/App.framework"
codesign --force --sign - \
  --requirements '=designated => identifier "com.weline.luckyPacketAssistant"' \
  --preserve-metadata=entitlements,flags,runtime \
  "$app_path"

codesign --verify --deep --strict --verbose=2 "$app_path"
codesign -d -r- "$app_path" 2>&1 | \
  grep -F 'designated => identifier "com.weline.luckyPacketAssistant"'
