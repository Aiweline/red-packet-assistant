#include "automation_policy.h"
#include "visual_detection.h"

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <vector>

namespace {

visual_detection::BgraPixel Pixel(uint8_t red,
                                  uint8_t green,
                                  uint8_t blue) {
  return {blue, green, red, 255};
}

void FillRectangle(std::vector<visual_detection::BgraPixel>* pixels,
                   int image_width,
                   int left,
                   int top,
                   int right,
                   int bottom,
                   visual_detection::BgraPixel color) {
  for (int y = top; y < bottom; ++y) {
    for (int x = left; x < right; ++x) {
      (*pixels)[static_cast<size_t>(y) * image_width + x] = color;
    }
  }
}

}  // namespace

int main() {
  using automation_policy::Bounds;
  using automation_policy::Point;

  assert(automation_policy::IsPacketLabel(L"恭喜发财 微信红包"));
  assert(!automation_policy::IsPacketLabel(L"微信红包 已被领完"));
  assert(automation_policy::IsOpenLabel(L"开"));
  assert(!automation_policy::IsOpenLabel(L"开启自动处理"));
  assert(automation_policy::IsPopupCloseLabel(L"关闭"));
  assert(!automation_policy::IsPopupCloseLabel(L"关闭窗口后继续"));

  assert(automation_policy::IsWeChatExecutablePath(
      L"C:\\Program Files\\Tencent\\Weixin.exe"));
  assert(automation_policy::IsWeChatExecutablePath(L"wechat.exe"));
  assert(automation_policy::IsPrimaryWeChatExecutablePath(L"wechat.exe"));
  assert(!automation_policy::IsPrimaryWeChatExecutablePath(
      L"WeChatAppEx.exe"));
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
  assert(automation_policy::IsPopupClosePosition(
      Bounds{960, 880, 1020, 940}, window));
  assert(!automation_policy::IsPopupClosePosition(
      Bounds{1820, 55, 1870, 105}, window));

  Point fallback;
  const Bounds popup_header{850, 250, 1130, 300};
  assert(automation_policy::FallbackOpenPoint(popup_header, window,
                                              &fallback));
  assert(fallback.x == 990);
  assert(fallback.y >= 425 && fallback.y <= 744);

  const Bounds sidebar_header{500, 250, 650, 300};
  assert(!automation_policy::FallbackOpenPoint(sidebar_header, window,
                                               &fallback));

  constexpr int image_width = 1000;
  constexpr int image_height = 700;
  std::vector<visual_detection::BgraPixel> pixels(
      static_cast<size_t>(image_width) * image_height,
      Pixel(248, 248, 248));
  FillRectangle(&pixels, image_width, 350, 280, 680, 400,
                Pixel(238, 171, 84));
  FillRectangle(&pixels, image_width, 385, 305, 440, 365,
                Pixel(232, 74, 60));
  FillRectangle(&pixels, image_width, 700, 180, 900, 250,
                Pixel(145, 235, 155));
  const visual_detection::ImageView image{pixels.data(), image_width,
                                           image_height, image_width};
  const std::vector<visual_detection::Region> packet_regions =
      visual_detection::FindPacketRegions(image);
  assert(packet_regions.size() == 1);
  assert(packet_regions.front().bounds.left >= 346);
  assert(packet_regions.front().bounds.right <= 684);
  assert(packet_regions.front().fill_ratio > 0.70);

  std::fill(pixels.begin(), pixels.end(), Pixel(248, 248, 248));
  FillRectangle(&pixels, image_width, 350, 280, 680, 400,
                Pixel(255, 205, 160));
  assert(visual_detection::FindPacketRegions(image).empty());

  std::fill(pixels.begin(), pixels.end(), Pixel(248, 248, 248));
  FillRectangle(&pixels, image_width, 350, 170, 680, 290,
                Pixel(238, 171, 84));
  constexpr int open_x = 520;
  constexpr int open_y = 430;
  constexpr int open_radius = 42;
  for (int y = open_y - open_radius; y <= open_y + open_radius; ++y) {
    for (int x = open_x - open_radius; x <= open_x + open_radius; ++x) {
      const int delta_x = x - open_x;
      const int delta_y = y - open_y;
      if (delta_x * delta_x + delta_y * delta_y <=
          open_radius * open_radius) {
        pixels[static_cast<size_t>(y) * image_width + x] =
            Pixel(210, 165, 80);
      }
    }
  }
  const std::vector<visual_detection::Region> open_regions =
      visual_detection::FindOpenButtonRegions(image);
  assert(open_regions.size() == 1);
  assert(automation_policy::CenterX(open_regions.front().bounds) >= 516);
  assert(automation_policy::CenterX(open_regions.front().bounds) <= 524);

  std::fill(pixels.begin(), pixels.end(), Pixel(248, 248, 248));
  for (int y = open_y - open_radius; y <= open_y + open_radius; ++y) {
    for (int x = open_x - open_radius; x <= open_x + open_radius; ++x) {
      const int distance = (x - open_x) * (x - open_x) +
                           (y - open_y) * (y - open_y);
      if (distance <= open_radius * open_radius &&
          distance >= (open_radius - 7) * (open_radius - 7)) {
        pixels[static_cast<size_t>(y) * image_width + x] =
            Pixel(210, 165, 80);
      }
    }
  }
  assert(visual_detection::FindOpenButtonRegions(image).empty());
  const std::vector<visual_detection::Region> close_regions =
      visual_detection::FindCloseButtonRegions(image);
  assert(close_regions.size() == 1);
  assert(automation_policy::CenterY(close_regions.front().bounds) >= 426);
  assert(automation_policy::CenterY(close_regions.front().bounds) <= 434);

  std::cout << "Windows automation policy tests passed\n";
  return 0;
}
