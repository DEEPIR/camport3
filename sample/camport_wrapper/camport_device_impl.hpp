#pragma once
#include "../common/common.hpp"
#include "camport_wrapper.hpp"

namespace deepir {
namespace camport_wrapper {

struct dev_data {
  TY_DEVICE_BASE_INFO base_info;
  TY_INTERFACE_HANDLE hIface = nullptr;
  TY_DEV_HANDLE hDevice = nullptr;
  std::vector<std::vector<char>> frameBuffer{2};
  TY_FRAME_DATA frame_data;
  bool rgb_support{false};
  bool ir_left_support{false};
  bool ir_right_support{false};
  bool depth_support{false};
  int width{0};
  int height{0};
  uint64_t index{0};
  ~dev_data() {
    if (hDevice) {
      TYStopCapture(hDevice);
      TYCloseDevice(hDevice);
    }
    if (hIface) {
      TYCloseInterface(hIface);
    }
  }
};

class camport_device_impl : public camport_device {
public:
  camport_device_impl(std::shared_ptr<dev_data> dev_data)
      : dev_data_(dev_data) {
    if (!dev_data_) {
      throw std::runtime_error("dev_data is nullptr");
    }
  }
  std::string id() override;
  void start() override;
  mat next_depth_frame() override;

private:
  std::shared_ptr<dev_data> dev_data_;
};

} // namespace camport_wrapper
} // namespace deepir