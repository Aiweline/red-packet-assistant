#include "automation_policy.h"

#include <cassert>
#include <iostream>

int main() {
  using automation_policy::Bounds;
  using automation_policy::Point;

  assert(automation_policy::IsPacketLabel(L"恭喜发财 微信红包"));
  assert(!automation_policy::IsPacketLabel(L"微信红包 已被领完"));
  assert(automation_policy::IsOpenLabel(L"开"));
  assert(!automation_policy::IsOpenLabel(L"开启自动处理"));

  assert(automation_policy::IsWeChatExecutablePath(
      L"C:\\Program Files\\Tencent\\Weixin.exe"));
  assert(automation_policy::IsWeChatExecutablePath(L"wechat.exe"));
  assert(!automation_policy::IsWeChatExecutablePath(L"红包助手.exe"));

  const Bounds window{480, 40, 1900, 1140};
  const Bounds packet_label{1030, 570, 1140, 610};
  const Bounds completed_label{1030, 500, 1160, 540};
  const Bounds distant_status{1030, 760, 1160, 800};
  assert(automation_policy::IsSamePacketCard(packet_label, completed_label,
                                              window));
  assert(!automation_policy::IsSamePacketCard(packet_label, distant_status,
                                               window));
  assert(automation_policy::IsPacketPosition(packet_label, window));
  assert(!automation_policy::IsPacketPosition(
      Bounds{560, 570, 700, 610}, window));
  assert(automation_policy::IsOpenPosition(
      Bounds{980, 520, 1040, 580}, window));
  assert(!automation_policy::IsOpenPosition(
      Bounds{540, 520, 600, 580}, window));

  Point fallback;
  const Bounds popup_header{850, 250, 1130, 300};
  assert(automation_policy::FallbackOpenPoint(popup_header, window,
                                              &fallback));
  assert(fallback.x == 990);
  assert(fallback.y >= 425 && fallback.y <= 744);

  const Bounds sidebar_header{500, 250, 650, 300};
  assert(!automation_policy::FallbackOpenPoint(sidebar_header, window,
                                               &fallback));

  std::cout << "Windows automation policy tests passed\n";
  return 0;
}
