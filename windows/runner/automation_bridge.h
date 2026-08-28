#ifndef RUNNER_AUTOMATION_BRIDGE_H_
#define RUNNER_AUTOMATION_BRIDGE_H_

#include <flutter/binary_messenger.h>
#include <flutter/method_channel.h>
#include <flutter/standard_method_codec.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <vector>

class AutomationBridge {
 public:
  explicit AutomationBridge(flutter::BinaryMessenger* messenger);
  ~AutomationBridge();

  AutomationBridge(const AutomationBridge&) = delete;
  AutomationBridge& operator=(const AutomationBridge&) = delete;

 private:
  flutter::EncodableMap Status();
  void SetEnabled(bool enabled);
  void WorkerLoop();
  bool ScanWeChat();

  struct VisualPacketSnapshot {
    long left = 0;
    long top = 0;
    long right = 0;
    long bottom = 0;
    uint64_t appearance_signature = 0;
  };

  std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_;
  std::atomic<bool> enabled_{false};
  std::atomic<bool> stop_{false};
  std::thread worker_;
  std::mutex state_mutex_;
  int detected_count_ = 0;
  int opened_count_ = 0;
  int64_t last_action_at_ = 0;
  std::wstring last_action_ = L"暂无记录";
  std::chrono::steady_clock::time_point packet_stage_until_{};
  std::chrono::steady_clock::time_point last_click_at_{};
  std::chrono::steady_clock::time_point popup_close_after_{};
  bool visual_baseline_ready_ = false;
  std::vector<VisualPacketSnapshot> previous_visual_packets_;
  std::set<std::wstring> detected_fingerprints_;
  std::set<std::wstring> processed_fingerprints_;
};

#endif  // RUNNER_AUTOMATION_BRIDGE_H_
