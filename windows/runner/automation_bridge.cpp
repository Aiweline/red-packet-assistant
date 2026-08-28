#include "automation_bridge.h"

#include "automation_policy.h"

#include <UIAutomation.h>
#include <Windows.h>
#include <shellapi.h>
#include <wrl/client.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <iterator>
#include <sstream>
#include <utility>
#include <vector>

namespace {

using Microsoft::WRL::ComPtr;

constexpr char kChannelName[] =
    "com.weline.lucky_packet_assistant/automation";

struct WindowSearch {
  HWND window = nullptr;
  long long score = 0;
};

std::wstring WindowExecutablePath(HWND window) {
  DWORD process_id = 0;
  GetWindowThreadProcessId(window, &process_id);
  if (process_id == 0) {
    return L"";
  }

  const HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE,
                                     process_id);
  if (process == nullptr) {
    return L"";
  }
  std::wstring path(32768, L'\0');
  DWORD size = static_cast<DWORD>(path.size());
  const bool queried =
      QueryFullProcessImageNameW(process, 0, path.data(), &size) != FALSE;
  CloseHandle(process);
  if (!queried) {
    return L"";
  }
  path.resize(size);
  return path;
}

BOOL CALLBACK FindWeChatWindowCallback(HWND window, LPARAM parameter) {
  if (!IsWindowVisible(window) || IsIconic(window)) {
    return TRUE;
  }

  RECT bounds{};
  if (!GetWindowRect(window, &bounds)) {
    return TRUE;
  }
  const long width = bounds.right - bounds.left;
  const long height = bounds.bottom - bounds.top;
  if (width < 500 || height < 350) {
    return TRUE;
  }

  wchar_t title[512] = {};
  GetWindowTextW(window, title, static_cast<int>(std::size(title)));
  const std::wstring value(title);
  const bool title_matches = value.find(L"微信") != std::wstring::npos ||
                             value.find(L"WeChat") != std::wstring::npos;
  const bool process_matches = automation_policy::IsWeChatExecutablePath(
      WindowExecutablePath(window));
  if (!title_matches && !process_matches) {
    return TRUE;
  }

  const long long area = static_cast<long long>(width) * height;
  const long long score = area + (process_matches ? (1LL << 60) : 0);
  auto* search = reinterpret_cast<WindowSearch*>(parameter);
  if (score > search->score) {
    search->window = window;
    search->score = score;
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

void AppendSearchPart(std::wstring* result, BSTR value) {
  if (value == nullptr) {
    return;
  }
  const std::wstring part(value, SysStringLen(value));
  SysFreeString(value);
  if (part.empty() || result->find(part) != std::wstring::npos) {
    return;
  }
  if (!result->empty()) {
    result->append(L"\n");
  }
  result->append(part);
}

std::wstring ElementSearchText(IUIAutomationElement* element) {
  std::wstring result;
  BSTR value = nullptr;
  if (SUCCEEDED(element->get_CurrentName(&value))) {
    AppendSearchPart(&result, value);
  }
  value = nullptr;
  if (SUCCEEDED(element->get_CurrentHelpText(&value))) {
    AppendSearchPart(&result, value);
  }
  value = nullptr;
  if (SUCCEEDED(element->get_CurrentAutomationId(&value))) {
    AppendSearchPart(&result, value);
  }

  ComPtr<IUIAutomationValuePattern> value_pattern;
  if (SUCCEEDED(element->GetCurrentPatternAs(
          UIA_ValuePatternId,
          IID_PPV_ARGS(value_pattern.GetAddressOf()))) &&
      value_pattern) {
    value = nullptr;
    if (SUCCEEDED(value_pattern->get_CurrentValue(&value))) {
      AppendSearchPart(&result, value);
    }
  }
  return result;
}

automation_policy::Bounds ElementBounds(IUIAutomationElement* element) {
  RECT value{};
  if (FAILED(element->get_CurrentBoundingRectangle(&value))) {
    return {};
  }
  return {value.left, value.top, value.right, value.bottom};
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

    ComPtr<IUIAutomationLegacyIAccessiblePattern> legacy;
    if (SUCCEEDED(current->GetCurrentPatternAs(
            UIA_LegacyIAccessiblePatternId,
            IID_PPV_ARGS(legacy.GetAddressOf()))) &&
        legacy && SUCCEEDED(legacy->DoDefaultAction())) {
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

bool ClickScreenPoint(HWND window, const POINT& point) {
  RECT window_bounds{};
  if (!GetWindowRect(window, &window_bounds) ||
      point.x < window_bounds.left || point.x > window_bounds.right ||
      point.y < window_bounds.top || point.y > window_bounds.bottom) {
    return false;
  }

  if (IsIconic(window)) {
    ShowWindow(window, SW_RESTORE);
  }
  SetForegroundWindow(window);
  Sleep(50);

  DWORD window_process = 0;
  DWORD hit_process = 0;
  GetWindowThreadProcessId(window, &window_process);
  const HWND hit_window = WindowFromPoint(point);
  if (hit_window == nullptr) {
    return false;
  }
  GetWindowThreadProcessId(hit_window, &hit_process);
  if (window_process == 0 || window_process != hit_process) {
    return false;
  }

  const int virtual_left = GetSystemMetrics(SM_XVIRTUALSCREEN);
  const int virtual_top = GetSystemMetrics(SM_YVIRTUALSCREEN);
  const int virtual_width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
  const int virtual_height = GetSystemMetrics(SM_CYVIRTUALSCREEN);
  if (virtual_width <= 1 || virtual_height <= 1) {
    return false;
  }

  const LONG normalized_x = static_cast<LONG>(
      static_cast<double>(point.x - virtual_left) * 65535.0 /
      static_cast<double>(virtual_width - 1));
  const LONG normalized_y = static_cast<LONG>(
      static_cast<double>(point.y - virtual_top) * 65535.0 /
      static_cast<double>(virtual_height - 1));

  INPUT inputs[3] = {};
  inputs[0].type = INPUT_MOUSE;
  inputs[0].mi.dx = normalized_x;
  inputs[0].mi.dy = normalized_y;
  inputs[0].mi.dwFlags =
      MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
  inputs[1].type = INPUT_MOUSE;
  inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
  inputs[2].type = INPUT_MOUSE;
  inputs[2].mi.dwFlags = MOUSEEVENTF_LEFTUP;
  return SendInput(static_cast<UINT>(std::size(inputs)), inputs,
                   static_cast<int>(sizeof(INPUT))) ==
         static_cast<UINT>(std::size(inputs));
}

bool ActivateElement(IUIAutomationTreeWalker* walker,
                     IUIAutomationElement* element,
                     HWND window) {
  if (InvokeElement(walker, element)) {
    return true;
  }

  if (IsIconic(window)) {
    ShowWindow(window, SW_RESTORE);
  }
  SetForegroundWindow(window);

  ComPtr<IUIAutomationElement> current(element);
  for (int depth = 0; depth < 7 && current; ++depth) {
    POINT point{};
    BOOL clickable = FALSE;
    if (SUCCEEDED(current->GetClickablePoint(&point, &clickable)) &&
        clickable && ClickScreenPoint(window, point)) {
      return true;
    }

    const automation_policy::Bounds bounds = ElementBounds(current.Get());
    RECT raw_window{};
    GetWindowRect(window, &raw_window);
    const automation_policy::Bounds window_bounds{
        raw_window.left, raw_window.top, raw_window.right, raw_window.bottom};
    if (automation_policy::IsUsableBounds(bounds, window_bounds)) {
      point.x = automation_policy::CenterX(bounds);
      point.y = automation_policy::CenterY(bounds);
      if (ClickScreenPoint(window, point)) {
        return true;
      }
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
    detected_fingerprints_.clear();
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
  if (FAILED(automation->get_RawViewWalker(walker.GetAddressOf())) &&
      FAILED(automation->get_ControlViewWalker(walker.GetAddressOf()))) {
    return false;
  }

  RECT raw_window_bounds{};
  if (!GetWindowRect(window, &raw_window_bounds)) {
    return false;
  }
  const automation_policy::Bounds window_bounds{
      raw_window_bounds.left, raw_window_bounds.top, raw_window_bounds.right,
      raw_window_bounds.bottom};

  const auto now = std::chrono::steady_clock::now();
  bool stage_active = false;
  std::chrono::steady_clock::time_point last_click;
  {
    std::lock_guard<std::mutex> lock(state_mutex_);
    stage_active = now < packet_stage_until_;
    last_click = last_click_at_;
  }

  struct Candidate {
    ComPtr<IUIAutomationElement> element;
    std::wstring label;
    std::wstring name;
    automation_policy::Bounds bounds;
  };

  std::vector<Candidate> packet_candidates;
  std::vector<Candidate> open_candidates;
  std::vector<Candidate> popup_headers;
  std::vector<automation_policy::Bounds> terminal_bounds;

  std::deque<ComPtr<IUIAutomationElement>> queue;
  queue.push_back(root);
  int inspected = 0;
  while (!queue.empty() && inspected++ < 6000) {
    ComPtr<IUIAutomationElement> element = queue.front();
    queue.pop_front();

    const std::wstring name = ElementName(element.Get());
    const std::wstring label = ElementSearchText(element.Get());
    const automation_policy::Bounds bounds = ElementBounds(element.Get());
    BOOL offscreen = FALSE;
    element->get_CurrentIsOffscreen(&offscreen);
    if (!offscreen &&
        automation_policy::IsUsableBounds(bounds, window_bounds)) {
      if (automation_policy::IsTerminalLabel(label) &&
          automation_policy::IsPacketPosition(bounds, window_bounds)) {
        terminal_bounds.push_back(bounds);
      }
      if (automation_policy::IsPacketLabel(label) &&
          automation_policy::IsPacketPosition(bounds, window_bounds)) {
        packet_candidates.push_back({element, label, name, bounds});
      }
      if (stage_active && (automation_policy::IsOpenLabel(name) ||
                           automation_policy::IsOpenLabel(label)) &&
          automation_policy::IsOpenPosition(bounds, window_bounds)) {
        open_candidates.push_back({element, label, name, bounds});
      }
      if (stage_active && automation_policy::IsPopupHeaderLabel(label)) {
        popup_headers.push_back({element, label, name, bounds});
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

  const auto try_candidate = [&](const Candidate& candidate,
                                 bool packet) -> bool {
    const std::wstring fingerprint = ElementFingerprint(
        candidate.element.Get(), packet ? L"packet" : L"open",
        candidate.label);
    bool processed = false;
    {
      std::lock_guard<std::mutex> lock(state_mutex_);
      processed = processed_fingerprints_.find(fingerprint) !=
                  processed_fingerprints_.end();
      if (packet &&
          detected_fingerprints_.insert(fingerprint).second) {
        ++detected_count_;
        last_action_ = L"已识别微信红包，正在尝试点击";
        last_action_at_ = CurrentMilliseconds();
        if (detected_fingerprints_.size() > 120) {
          detected_fingerprints_.clear();
          detected_fingerprints_.insert(fingerprint);
        }
      }
    }
    if (processed || now - last_click <= std::chrono::milliseconds(900)) {
      return false;
    }

    if (!enabled_) {
      return false;
    }

    if (!ActivateElement(walker.Get(), candidate.element.Get(), window)) {
      std::lock_guard<std::mutex> lock(state_mutex_);
      last_action_ = packet
                         ? L"已识别红包，但微信控件暂时无法点击"
                         : L"已识别“开”按钮，但微信控件暂时无法点击";
      last_action_at_ = CurrentMilliseconds();
      return false;
    }

    std::lock_guard<std::mutex> lock(state_mutex_);
    processed_fingerprints_.insert(fingerprint);
    if (processed_fingerprints_.size() > 120) {
      processed_fingerprints_.clear();
      processed_fingerprints_.insert(fingerprint);
    }
    if (packet) {
      packet_stage_until_ = now + std::chrono::seconds(8);
      last_action_ = L"已点击微信红包卡片";
    } else {
      packet_stage_until_ = {};
      last_action_ = L"已点击“开”红包按钮";
    }
    ++opened_count_;
    last_action_at_ = CurrentMilliseconds();
    last_click_at_ = now;
    return true;
  };

  std::sort(open_candidates.begin(), open_candidates.end(),
            [](const Candidate& left, const Candidate& right) {
              return automation_policy::Width(left.bounds) *
                         automation_policy::Height(left.bounds) >
                     automation_policy::Width(right.bounds) *
                         automation_policy::Height(right.bounds);
            });
  for (const Candidate& candidate : open_candidates) {
    if (try_candidate(candidate, false)) {
      return true;
    }
  }

  if (stage_active && terminal_bounds.empty() && !popup_headers.empty() &&
      now - last_click > std::chrono::milliseconds(900)) {
    std::sort(popup_headers.begin(), popup_headers.end(),
              [](const Candidate& left, const Candidate& right) {
                return automation_policy::Height(left.bounds) >
                       automation_policy::Height(right.bounds);
              });
    automation_policy::Point fallback;
    const Candidate& header = popup_headers.front();
    if (automation_policy::FallbackOpenPoint(header.bounds, window_bounds,
                                             &fallback)) {
      std::wstringstream fingerprint_stream;
      fingerprint_stream << L"open-fallback|" << header.label << L'|'
                         << fallback.x << L':' << fallback.y;
      const std::wstring fingerprint = fingerprint_stream.str();
      bool processed = false;
      {
        std::lock_guard<std::mutex> lock(state_mutex_);
        processed = processed_fingerprints_.find(fingerprint) !=
                    processed_fingerprints_.end();
      }
      const POINT point{fallback.x, fallback.y};
      if (!processed && enabled_ && ClickScreenPoint(window, point)) {
        std::lock_guard<std::mutex> lock(state_mutex_);
        processed_fingerprints_.insert(fingerprint);
        packet_stage_until_ = {};
        last_click_at_ = now;
        ++opened_count_;
        last_action_ = L"红包弹窗已确认，已点击圆形“开”按钮";
        last_action_at_ = CurrentMilliseconds();
        return true;
      }
    }
  }

  std::sort(packet_candidates.begin(), packet_candidates.end(),
            [](const Candidate& left, const Candidate& right) {
              return left.bounds.bottom > right.bounds.bottom;
            });
  for (const Candidate& candidate : packet_candidates) {
    const bool completed = std::any_of(
        terminal_bounds.begin(), terminal_bounds.end(),
        [&](const automation_policy::Bounds& terminal) {
          return automation_policy::IsSamePacketCard(
              candidate.bounds, terminal, window_bounds);
        });
    if (!completed && try_candidate(candidate, true)) {
      return true;
    }
  }

  if (packet_candidates.empty()) {
    std::lock_guard<std::mutex> lock(state_mutex_);
    if (last_action_ == L"助手已启动") {
      last_action_ = L"微信窗口已连接，已扫描 " +
                     std::to_wstring(inspected) +
                     L" 个 UI 元素，暂未读取到红包文字";
      last_action_at_ = CurrentMilliseconds();
    }
  }
  return false;
}
