#pragma once
#include <memory>
#include <opencv2/opencv.hpp>
namespace deepir {
namespace camport_wrapper {
struct mat {
  cv::Mat content;
  uint64_t seq{0};
};
class camport_wrapper {
public:
  camport_wrapper();
  ~camport_wrapper();
  mat next_depth_frame();
  void init();
  void start();

private:
  class impl;
  friend class impl;
  std::unique_ptr<impl> pimpl;
};
} // namespace camport_wrapper
} // namespace deepir