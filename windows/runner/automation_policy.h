#ifndef RUNNER_AUTOMATION_POLICY_H_
#define RUNNER_AUTOMATION_POLICY_H_

#include <algorithm>
#include <cwctype>
#include <initializer_list>
#include <string>
#include <string_view>

namespace automation_policy {

struct Bounds {
  long left = 0;
  long top = 0;
  long right = 0;
  long bottom = 0;
};

struct Point {
  long x = 0;
  long y = 0;
};

inline long Width(const Bounds& value) {
  return value.right - value.left;
}

inline long Height(const Bounds& value) {
  return value.bottom - value.top;
}

inline long CenterX(const Bounds& value) {
  return value.left + Width(value) / 2;
}

inline long CenterY(const Bounds& value) {
  return value.top + Height(value) / 2;
}

inline bool ContainsAny(
    std::wstring_view text,
    std::initializer_list<std::wstring_view> markers) {
  for (const std::wstring_view marker : markers) {
    if (text.find(marker) != std::wstring_view::npos) {
      return true;
    }
  }
  return false;
}

inline bool IsTerminalLabel(std::wstring_view text) {
  return ContainsAny(text, {L"已领取", L"已被领完", L"已领完", L"已抢完",
                            L"已过期", L"已退款", L"红包派完了",
                            L"来晚一步"});
}

inline bool IsPacketLabel(std::wstring_view text) {
  return !IsTerminalLabel(text) &&
         ContainsAny(text, {L"微信红包", L"领取红包", L"查看红包",
                            L"发来一个红包"});
}

inline bool IsOpenLabel(std::wstring_view text) {
  return text == L"开" || text == L"拆红包" || text == L"领取红包";
}

inline bool IsPopupHeaderLabel(std::wstring_view text) {
  return ContainsAny(text, {L"发出的红包", L"给你发了一个红包",
                            L"给你发的红包"});
}

inline bool IsPopupCloseLabel(std::wstring_view text) {
  return text == L"关闭" || text == L"返回";
}

inline bool IsUsableBounds(const Bounds& value, const Bounds& window) {
  return Width(value) >= 4 && Height(value) >= 4 &&
         CenterX(value) >= window.left && CenterX(value) <= window.right &&
         CenterY(value) >= window.top && CenterY(value) <= window.bottom;
}

inline bool IsPacketPosition(const Bounds& value, const Bounds& window) {
  if (!IsUsableBounds(value, window) || Width(window) <= 0 ||
      Height(window) <= 0) {
    return false;
  }
  const double normalized_x =
      static_cast<double>(CenterX(value) - window.left) / Width(window);
  const double normalized_y =
      static_cast<double>(CenterY(value) - window.top) / Height(window);
  return normalized_x >= 0.26 && normalized_x <= 0.98 &&
         normalized_y >= 0.08 && normalized_y <= 0.95 &&
         Width(value) <= Width(window) * 65 / 100 &&
         Height(value) <= Height(window) * 45 / 100;
}

inline bool IsOpenPosition(const Bounds& value, const Bounds& window) {
  if (!IsUsableBounds(value, window) || Width(window) <= 0 ||
      Height(window) <= 0) {
    return false;
  }
  const double normalized_x =
      static_cast<double>(CenterX(value) - window.left) / Width(window);
  const double normalized_y =
      static_cast<double>(CenterY(value) - window.top) / Height(window);
  return normalized_x >= 0.3 && normalized_x <= 0.78 &&
         normalized_y >= 0.22 && normalized_y <= 0.78;
}

inline bool IsPopupClosePosition(const Bounds& value, const Bounds& window) {
  if (!IsUsableBounds(value, window) || Width(window) <= 0 ||
      Height(window) <= 0) {
    return false;
  }
  const double normalized_x =
      static_cast<double>(CenterX(value) - window.left) / Width(window);
  const double normalized_y =
      static_cast<double>(CenterY(value) - window.top) / Height(window);
  const long shorter_side = std::min(Width(window), Height(window));
  const long width = Width(value);
  const long height = Height(value);
  return normalized_x >= 0.22 && normalized_x <= 0.82 &&
         normalized_y >= 0.50 && normalized_y <= 0.97 && width >= 18 &&
         height >= 18 && width <= shorter_side * 18 / 100 &&
         height <= shorter_side * 18 / 100 && width * 100 >= height * 60 &&
         width * 100 <= height * 165;
}

inline bool IsSamePacketCard(const Bounds& packet,
                             const Bounds& terminal,
                             const Bounds& window) {
  const long vertical_distance = CenterY(terminal) - CenterY(packet);
  const long horizontal_distance =
      std::abs(CenterX(terminal) - CenterX(packet));
  const long horizontal_limit =
      std::clamp(Width(window) / 5, 180L, 320L);
  return vertical_distance >= -170 && vertical_distance <= 110 &&
         horizontal_distance <= horizontal_limit;
}

inline bool FallbackOpenPoint(const Bounds& header,
                              const Bounds& window,
                              Point* result) {
  if (result == nullptr || Width(window) <= 0 || Height(window) <= 0) {
    return false;
  }
  const double normalized_x =
      static_cast<double>(CenterX(header) - window.left) / Width(window);
  const double normalized_y =
      static_cast<double>(CenterY(header) - window.top) / Height(window);
  if (normalized_x < 0.2 || normalized_x > 0.85 || normalized_y < 0.12 ||
      normalized_y > 0.58) {
    return false;
  }

  const long minimum_y = window.top + Height(window) * 35 / 100;
  const long maximum_y = window.top + Height(window) * 64 / 100;
  result->x = CenterX(header);
  result->y = std::clamp(
      CenterY(header) + Height(window) * 28 / 100, minimum_y, maximum_y);
  return true;
}

inline bool EqualsCaseInsensitive(std::wstring_view left,
                                  std::wstring_view right) {
  if (left.size() != right.size()) {
    return false;
  }
  for (size_t index = 0; index < left.size(); ++index) {
    if (std::towlower(left[index]) != std::towlower(right[index])) {
      return false;
    }
  }
  return true;
}

inline bool IsWeChatExecutablePath(std::wstring_view path) {
  const size_t separator = path.find_last_of(L"\\/");
  const std::wstring_view file_name =
      separator == std::wstring_view::npos ? path : path.substr(separator + 1);
  return EqualsCaseInsensitive(file_name, L"WeChat.exe") ||
         EqualsCaseInsensitive(file_name, L"Weixin.exe") ||
         EqualsCaseInsensitive(file_name, L"WeChatAppEx.exe");
}

inline bool IsPrimaryWeChatExecutablePath(std::wstring_view path) {
  const size_t separator = path.find_last_of(L"\\/");
  const std::wstring_view file_name =
      separator == std::wstring_view::npos ? path : path.substr(separator + 1);
  return EqualsCaseInsensitive(file_name, L"WeChat.exe") ||
         EqualsCaseInsensitive(file_name, L"Weixin.exe");
}

}  // namespace automation_policy

#endif  // RUNNER_AUTOMATION_POLICY_H_
