#ifndef RUNNER_VISUAL_DETECTION_H_
#define RUNNER_VISUAL_DETECTION_H_

#include "automation_policy.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace visual_detection {

struct BgraPixel {
  uint8_t blue = 0;
  uint8_t green = 0;
  uint8_t red = 0;
  uint8_t alpha = 0;
};

static_assert(sizeof(BgraPixel) == 4, "BGRA pixels must be four bytes");

struct ImageView {
  const BgraPixel* pixels = nullptr;
  int width = 0;
  int height = 0;
  int stride_pixels = 0;
};

struct Region {
  automation_policy::Bounds bounds;
  size_t matching_samples = 0;
  double fill_ratio = 0.0;
  uint64_t appearance_signature = 0;
};

inline bool IsValid(const ImageView& image) {
  return image.pixels != nullptr && image.width > 0 && image.height > 0 &&
         image.stride_pixels >= image.width;
}

inline const BgraPixel& PixelAt(const ImageView& image, int x, int y) {
  return image.pixels[y * image.stride_pixels + x];
}

inline bool IsPacketOrange(const BgraPixel& pixel) {
  const int red = pixel.red;
  const int green = pixel.green;
  const int blue = pixel.blue;
  return red >= 215 && green >= 115 && green <= 215 && blue <= 150 &&
         red - green >= 28 && green - blue >= 24;
}

inline bool IsOpenButtonGold(const BgraPixel& pixel) {
  const int red = pixel.red;
  const int green = pixel.green;
  const int blue = pixel.blue;
  return red >= 170 && green >= 110 && green <= 225 && blue <= 160 &&
         red - green >= 12 && red - green <= 105 && green - blue >= 18;
}

inline uint64_t AppearanceSignature(const ImageView& image,
                                    const automation_policy::Bounds& bounds) {
  constexpr int kColumns = 20;
  constexpr int kRows = 8;
  constexpr uint64_t kOffset = 1469598103934665603ULL;
  constexpr uint64_t kPrime = 1099511628211ULL;
  uint64_t hash = kOffset;
  for (int row = 0; row < kRows; ++row) {
    const int y = std::clamp(
        static_cast<int>(bounds.top +
                         (2 * row + 1) *
                             automation_policy::Height(bounds) /
                             (2 * kRows)),
        0, image.height - 1);
    for (int column = 0; column < kColumns; ++column) {
      const int x = std::clamp(
          static_cast<int>(bounds.left +
                           (2 * column + 1) *
                               automation_policy::Width(bounds) /
                               (2 * kColumns)),
          0, image.width - 1);
      const BgraPixel& pixel = PixelAt(image, x, y);
      const uint16_t quantized = static_cast<uint16_t>(
          ((pixel.red >> 5) << 6) | ((pixel.green >> 5) << 3) |
          (pixel.blue >> 5));
      hash ^= static_cast<uint8_t>(quantized & 0xff);
      hash *= kPrime;
      hash ^= static_cast<uint8_t>(quantized >> 8);
      hash *= kPrime;
    }
  }
  return hash;
}

template <typename Matcher>
std::vector<Region> FindConnectedRegions(
    const ImageView& image,
    const automation_policy::Bounds& search_bounds,
    Matcher matcher) {
  if (!IsValid(image)) {
    return {};
  }

  constexpr int kSampleStep = 2;
  const int grid_width = (image.width + kSampleStep - 1) / kSampleStep;
  const int grid_height = (image.height + kSampleStep - 1) / kSampleStep;
  std::vector<uint8_t> mask(static_cast<size_t>(grid_width) * grid_height, 0);

  const int first_grid_x =
      std::max(0L, search_bounds.left) / kSampleStep;
  const int first_grid_y =
      std::max(0L, search_bounds.top) / kSampleStep;
  const int last_grid_x = std::min(
      grid_width, (static_cast<int>(std::min<long>(image.width,
                                                   search_bounds.right)) +
                   kSampleStep - 1) /
                      kSampleStep);
  const int last_grid_y = std::min(
      grid_height, (static_cast<int>(std::min<long>(image.height,
                                                    search_bounds.bottom)) +
                    kSampleStep - 1) /
                       kSampleStep);

  for (int grid_y = first_grid_y; grid_y < last_grid_y; ++grid_y) {
    const int y = std::min(grid_y * kSampleStep + 1, image.height - 1);
    for (int grid_x = first_grid_x; grid_x < last_grid_x; ++grid_x) {
      const int x = std::min(grid_x * kSampleStep + 1, image.width - 1);
      if (matcher(PixelAt(image, x, y))) {
        mask[static_cast<size_t>(grid_y) * grid_width + grid_x] = 1;
      }
    }
  }

  std::vector<Region> result;
  std::vector<int> pending;
  for (int grid_y = first_grid_y; grid_y < last_grid_y; ++grid_y) {
    for (int grid_x = first_grid_x; grid_x < last_grid_x; ++grid_x) {
      const int start = grid_y * grid_width + grid_x;
      if (mask[start] == 0) {
        continue;
      }

      pending.clear();
      pending.push_back(start);
      mask[start] = 0;
      int minimum_x = grid_x;
      int maximum_x = grid_x;
      int minimum_y = grid_y;
      int maximum_y = grid_y;
      size_t count = 0;

      while (!pending.empty()) {
        const int current = pending.back();
        pending.pop_back();
        const int current_x = current % grid_width;
        const int current_y = current / grid_width;
        minimum_x = std::min(minimum_x, current_x);
        maximum_x = std::max(maximum_x, current_x);
        minimum_y = std::min(minimum_y, current_y);
        maximum_y = std::max(maximum_y, current_y);
        ++count;

        const auto enqueue = [&](int neighbour) {
          if (mask[neighbour] != 0) {
            mask[neighbour] = 0;
            pending.push_back(neighbour);
          }
        };
        if (current_x > first_grid_x) {
          enqueue(current - 1);
        }
        if (current_x + 1 < last_grid_x) {
          enqueue(current + 1);
        }
        if (current_y > first_grid_y) {
          enqueue(current - grid_width);
        }
        if (current_y + 1 < last_grid_y) {
          enqueue(current + grid_width);
        }
      }

      const size_t component_box_samples =
          static_cast<size_t>(maximum_x - minimum_x + 1) *
          static_cast<size_t>(maximum_y - minimum_y + 1);
      Region region;
      region.bounds = {
          minimum_x * kSampleStep,
          minimum_y * kSampleStep,
          std::min((maximum_x + 1) * kSampleStep, image.width),
          std::min((maximum_y + 1) * kSampleStep, image.height),
      };
      region.matching_samples = count;
      region.fill_ratio = component_box_samples == 0
                              ? 0.0
                              : static_cast<double>(count) /
                                    component_box_samples;
      result.push_back(region);
    }
  }
  return result;
}

inline std::vector<Region> FindPacketRegions(const ImageView& image) {
  if (!IsValid(image)) {
    return {};
  }
  const automation_policy::Bounds search_bounds{
      image.width * 28L / 100L,
      image.height * 8L / 100L,
      image.width * 99L / 100L,
      image.height * 91L / 100L,
  };
  std::vector<Region> regions =
      FindConnectedRegions(image, search_bounds, IsPacketOrange);
  const long minimum_width = std::max(120L, image.width * 12L / 100L);
  const long maximum_width = image.width * 65L / 100L;
  const long minimum_height = std::max(48L, image.height * 5L / 100L);
  const long maximum_height = image.height * 30L / 100L;
  regions.erase(
      std::remove_if(regions.begin(), regions.end(), [&](const Region& region) {
        const long width = automation_policy::Width(region.bounds);
        const long height = automation_policy::Height(region.bounds);
        return width < minimum_width || width > maximum_width ||
               height < minimum_height || height > maximum_height ||
               width * 100 < height * 180 || width * 100 > height * 550 ||
               region.fill_ratio < 0.46;
      }),
      regions.end());
  for (Region& region : regions) {
    region.appearance_signature = AppearanceSignature(image, region.bounds);
  }
  std::sort(regions.begin(), regions.end(), [](const Region& left,
                                                const Region& right) {
    return left.bounds.top < right.bounds.top;
  });
  return regions;
}

inline std::vector<Region> FindOpenButtonRegions(const ImageView& image) {
  if (!IsValid(image)) {
    return {};
  }
  const automation_policy::Bounds search_bounds{
      image.width * 20L / 100L,
      image.height * 18L / 100L,
      image.width * 85L / 100L,
      image.height * 85L / 100L,
  };
  std::vector<Region> regions =
      FindConnectedRegions(image, search_bounds, IsOpenButtonGold);
  const long shorter_side = std::min(image.width, image.height);
  const long minimum_diameter = std::max(28L, shorter_side * 3L / 100L);
  const long maximum_diameter = shorter_side * 16L / 100L;
  regions.erase(
      std::remove_if(regions.begin(), regions.end(), [&](const Region& region) {
        const long width = automation_policy::Width(region.bounds);
        const long height = automation_policy::Height(region.bounds);
        return width < minimum_diameter || width > maximum_diameter ||
               height < minimum_diameter || height > maximum_diameter ||
               width * 100 < height * 70 || width * 100 > height * 138 ||
               region.fill_ratio < 0.34;
      }),
      regions.end());
  for (Region& region : regions) {
    region.appearance_signature = AppearanceSignature(image, region.bounds);
  }
  std::sort(regions.begin(), regions.end(), [](const Region& left,
                                                const Region& right) {
    return left.matching_samples > right.matching_samples;
  });
  return regions;
}

}  // namespace visual_detection

#endif  // RUNNER_VISUAL_DETECTION_H_
