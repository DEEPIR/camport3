#pragma once
#include "camport_wrapper.hpp"
#include "camport_device_impl.hpp"

namespace deepir{
namespace camport_wrapper {




class camport_wrapper_impl : public camport_wrapper {
public:
  std::vector<std::shared_ptr<camport_device>> find_devices(uint32_t device_num) override;
private:
  void init_device(const TY_DEVICE_BASE_INFO& selectedDev);
private:

  std::map<std::string, std::shared_ptr<dev_data>> selected_devs_;
};

}
}