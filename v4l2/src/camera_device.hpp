#pragma once
#include <deepir/util/thread_safe_container.hpp>
#include <deque>
#include <filesystem>
#include <opencv2/opencv.hpp>
#include <optional>
struct device_buffer {
  void *start = nullptr;
  size_t length{0};
};

#define CLEAR(x) memset(&(x), 0, sizeof(x))
struct camera_mat {
  cv::Mat content;
  uint64_t seq;
};
class camera_device {
public:
  explicit camera_device(const std::filesystem::path &p);
  ~camera_device();
  int get_fd() const { return fd_; }
  void stop_capturing(void);
  void start_capturing(void);
  std::optional<camera_mat> capture_frame();
  std::string get_name() const { return dev_path_.string(); }

private:
  int open_device(const std::filesystem::path &dev_path);
  bool wait_fd_ready(int sec, int fd);
  void init_device(int dev_fd);
  void uninit_device(void);
  void close_device(int dev_fd);
  void init_mmap(void);
  std::filesystem::path dev_path_;
  int fd_;
  const uint32_t buffer_size_ = 4;
  std::vector<device_buffer> device_buffers_;
  uint64_t frame_index_ = 0;
};
