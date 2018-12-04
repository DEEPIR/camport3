#pragma once
#include <memory>
#include <opencv2/opencv.hpp>
namespace deepir {
namespace camport_wrapper {
struct mat {
  cv::Mat content;
  uint64_t seq{0};
};
class camport_device {
public:
  virtual std::string id() = 0;
  virtual void start() = 0;
  virtual mat next_depth_frame() = 0;
  virtual ~camport_device() = default;
};

class camport_wrapper {
public:
  virtual std::vector<std::shared_ptr<camport_device>>
  find_devices(uint32_t device_num) = 0;
  virtual ~camport_wrapper() = default;
};

std::shared_ptr<camport_wrapper> get_wrapper();
} // namespace camport_wrapper
} // namespace deepir
