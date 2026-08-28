#include "automation_bridge.h"

#include <UIAutomation.h>
#include <Windows.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <chrono>
#include <deque>
#include <initializer_list>
#include <iterator>
#include <sstream>
#include <utility>

namespace {

using Microsoft::WRL::ComPtr;

constexpr char kChannelName[] =
    "com.weline.lucky_packet_assistant/automation";

struct WindowSearch {
  HWND window = nullptr;
};

bool ContainsAny(const std::wstring& text,
                 const std::initializer_list<const wchar_t*>& markers) {
  for (const wchar_t* marker : markers) {
    if (text.find(marker) != std::wstring::npos) {
      return true;
    }
  }
  return false;
}

BOOL CALLBACK FindWeChatWindowCallback(HWND window, LPARAM parameter) {
  if (!IsWindowVisible(window)) {
    return TRUE;
  }
  wchar_t title[512] = {};
  GetWindowTextW(window, title, static_cast<int>(std::size(title)));
  const std::wstring value(title);
  if (value.find(L"微信") != std::wstring::npos ||
      value.find(L"WeChat") != std::wstring::npos) {
    reinterpret_cast<WindowSearch*>(parameter)->window = window;
    return FALSE;
  }
  return TRUE;
}

HWND FindWeChatWindow() {
  WindowSearch search;
  EnumWindows(FindWeChatWindowCallback, reinterpret_cast<LPARAM>(&search));
  return search.window;
}

std::wstring ElementName(IUIAutomationElement* element) {
  BSTR raw_name = nullptr;
  if (FAILED(element->get_CurrentName(&raw_name)) || raw_name == nullptr) {
    return L"";
  }
  std::wstring name(raw_name, SysStringLen(raw_name));
  SysFreeString(raw_name);
  return name;
}

std::wstring ElementFingerprint(IUIAutomationElement* element,
                                const std::wstring& kind,
                                const std::wstring& label) {
  RECT bounds{};
  element->get_CurrentBoundingRectangle(&bounds);
  std::wstringstream stream;
  stream << kind << L'|' << label << L'|' << bounds.left << L':' << bounds.top
         << L':' << bounds.right << L':' << bounds.bottom;
  return stream.str();
}

bool InvokeElement(IUIAutomationTreeWalker* walker,
                   IUIAutomationElement* element) {
  ComPtr<IUIAutomationElement> current(element);
  for (int depth = 0; depth < 7 && current; ++depth) {
    ComPtr<IUIAutomationInvokePattern> invoke;
    if (SUCCEEDED(current->GetCurrentPatternAs(
            UIA_InvokePatternId, IID_PPV_ARGS(invoke.GetAddressOf()))) &&
        invoke && SUCCEEDED(invoke->Invoke())) {
      return true;
    }
    ComPtr<IUIAutomationElement> parent;
    if (FAILED(walker->GetParentElement(current.Get(), parent.GetAddressOf()))) {
      break;
    }
    current = std::move(parent);
  }
  return false;
}

std::string Utf8FromWide(const std::wstring& value) {
  if (value.empty()) {
    return "";
  }
  const int size = WideCharToMultiByte(CP_UTF8, 0, value.data(),
                                       static_cast<int>(value.size()), nullptr,
                                       0, nullptr, nullptr);
  std::string result(size, '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(),
                      static_cast<int>(value.size()), result.data(), size,
                      nullptr, nullptr);
  return result;
}

int64_t CurrentMilliseconds() {
  return std::chrono::duration_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now().time_since_epoch())
      .count();
}

}  // namespace

AutomationBridge::AutomationBridge(flutter::BinaryMessenger* messenger) {
  channel_ = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
      messenger, kChannelName, &flutter::StandardMethodCodec::GetInstance());
  channel_->SetMethodCallHandler(
      [this](const auto& call, auto result) {
        if (call.method_name() == "getStatus") {
          result->Success(flutter::EncodableValue(Status()));
          return;
        }
        if (call.method_name() == "setEnabled") {
          const auto* arguments = std::get_if<flutter::EncodableMap>(
              call.arguments());
          bool requested = false;
          if (arguments != nullptr) {
            const auto entry = arguments->find(
                flutter::EncodableValue("enabled"));
            if (entry != arguments->end()) {
              if (const auto* value = std::get_if<bool>(&entry->second)) {
                requested = *value;
              }
            }
          }
          SetEnabled(requested);
          result->Success();
          return;
        }
        if (call.method_name() == "openSettings") {
          ShellExecuteW(nullptr, L"open", L"ms-settings:easeofaccess", nullptr,
                        nullptr, SW_SHOWNORMAL);
          result->Success();
          return;
        }
        if (call.method_name() == "openWeChat") {
          const auto launched = reinterpret_cast<INT_PTR>(ShellExecuteW(
              nullptr, L"open", L"weixin://", nullptr, nullptr, SW_SHOWNORMAL));
          if (launched <= 32) {
            result->Error("wechat_open_failed", "未检测到可用的微信协议");
          } else {
            result->Success();
          }
          return;
        }
        result->NotImplemented();
      });
  worker_ = std::thread(&AutomationBridge::WorkerLoop, this);
}

AutomationBridge::~AutomationBridge() {
  stop_ = true;
  if (worker_.joinable()) {
    worker_.join();
  }
}

flutter::EncodableMap AutomationBridge::Status() {
  std::lock_guard<std::mutex> lock(state_mutex_);
  const bool running = FindWeChatWindow() != nullptr;
  const std::string message = enabled_
                                  ? (running ? "正在监测微信窗口中的红包控件"
                                             : "自动化已开启，请先打开微信")
                                  : "Windows UI Automation 已就绪，当前关闭";
  return {
      {flutter::EncodableValue("platform"),
       flutter::EncodableValue("windows")},
      {flutter::EncodableValue("supported"), flutter::EncodableValue(true)},
      {flutter::EncodableValue("enabled"),
       flutter::EncodableValue(enabled_.load())},
      {flutter::EncodableValue("notificationAccess"),
       flutter::EncodableValue(true)},
      {flutter::EncodableValue("accessibilityAccess"),
       flutter::EncodableValue(true)},
      {flutter::EncodableValue("weChatAvailable"),
       flutter::EncodableValue(running)},
      {flutter::EncodableValue("statusMessage"),
       flutter::EncodableValue(message)},
      {flutter::EncodableValue("lastAction"),
       flutter::EncodableValue(Utf8FromWide(last_action_))},
      {flutter::EncodableValue("lastActionAt"),
       flutter::EncodableValue(last_action_at_)},
      {flutter::EncodableValue("detectedCount"),
       flutter::EncodableValue(detected_count_)},
      {flutter::EncodableValue("openedCount"),
       flutter::EncodableValue(opened_count_)},
  };
}

void AutomationBridge::SetEnabled(bool enabled) {
  enabled_ = enabled;
  std::lock_guard<std::mutex> lock(state_mutex_);
  last_action_ = enabled ? L"助手已启动" : L"助手已暂停";
  last_action_at_ = CurrentMilliseconds();
  if (!enabled) {
    processed_fingerprints_.clear();
    packet_stage_until_ = {};
  }
}

void AutomationBridge::WorkerLoop() {
  CoInitializeEx(nullptr, COINIT_MULTITHREADED);
  while (!stop_) {
    if (enabled_) {
      ScanWeChat();
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(enabled_ ? 650 : 250));
  }
  CoUninitialize();
}

bool AutomationBridge::ScanWeChat() {
  const HWND window = FindWeChatWindow();
  if (window == nullptr) {
    return false;
  }

  ComPtr<IUIAutomation> automation;
  if (FAILED(CoCreateInstance(CLSID_CUIAutomation, nullptr,
                              CLSCTX_INPROC_SERVER,
                              IID_PPV_ARGS(automation.GetAddressOf())))) {
    return false;
  }
  ComPtr<IUIAutomationElement> root;
  if (FAILED(automation->ElementFromHandle(window, root.GetAddressOf()))) {
    return false;
  }
  ComPtr<IUIAutomationTreeWalker> walker;
  if (FAILED(automation->get_ControlViewWalker(walker.GetAddressOf()))) {
    return false;
  }

  std::deque<ComPtr<IUIAutomationElement>> queue;
  queue.push_back(root);
  int inspected = 0;
  while (!queue.empty() && inspected++ < 1800) {
    ComPtr<IUIAutomationElement> element = queue.front();
    queue.pop_front();
    const std::wstring label = ElementName(element.Get());
    const bool terminal = ContainsAny(
        label, {L"已领取", L"已被领完", L"已领完", L"已过期", L"已退款"});
    const bool packet = !terminal && ContainsAny(
        label, {L"微信红包", L"领取红包", L"查看红包", L"发来一个红包"});
    const auto now = std::chrono::steady_clock::now();
    bool stage_active = false;
    std::chrono::steady_clock::time_point last_click;
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      stage_active = now < packet_stage_until_;
      last_click = last_click_at_;
    }
    const bool open = stage_active &&
                      (label == L"开" || label == L"拆红包" ||
                       label == L"领取红包");

    if (packet || open) {
      const std::wstring fingerprint = ElementFingerprint(
          element.Get(), packet ? L"packet" : L"open", label);
      bool unseen = false;
      {
        std::lock_guard<std::mutex> lock(state_mutex_);
        unseen = processed_fingerprints_.find(fingerprint) ==
                 processed_fingerprints_.end();
      }
      if (unseen && now - last_click > std::chrono::milliseconds(900) &&
          InvokeElement(walker.Get(), element.Get())) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        processed_fingerprints_.insert(fingerprint);
        if (processed_fingerprints_.size() > 120) {
          processed_fingerprints_.clear();
          processed_fingerprints_.insert(fingerprint);
        }
        if (packet) {
          ++detected_count_;
          packet_stage_until_ = now + std::chrono::seconds(8);
          last_action_ = L"已点击微信红包控件";
        } else {
          packet_stage_until_ = {};
          last_action_ = L"已点击“开”红包控件";
        }
        ++opened_count_;
        last_action_at_ = CurrentMilliseconds();
        last_click_at_ = now;
        return true;
      }
    }

    ComPtr<IUIAutomationElement> child;
    if (SUCCEEDED(walker->GetFirstChildElement(element.Get(),
                                                child.GetAddressOf()))) {
      while (child) {
        queue.push_back(child);
        ComPtr<IUIAutomationElement> sibling;
        if (FAILED(walker->GetNextSiblingElement(child.Get(),
                                                 sibling.GetAddressOf()))) {
          break;
        }
        child = std::move(sibling);
      }
    }
  }
  return false;
}
