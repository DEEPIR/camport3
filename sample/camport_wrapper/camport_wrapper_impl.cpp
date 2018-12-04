#include "camport_wrapper_impl.hpp"
#include "check_ok.hpp"
#include "../common/common.hpp"
#include <iostream>
#include <sstream>
using std::cout;
using std::endl;
namespace deepir{
namespace camport_wrapper {

void eventCallback(TY_EVENT_INFO *event_info, void *userdata) {
  if (event_info->eventId == TY_EVENT_DEVICE_OFFLINE) {
    LOGD("=== Event Callback: Device Offline!");
    // Note:
    //     Please set TY_BOOL_KEEP_ALIVE_ONOFF feature to false if you need to
    //     debug with breakpoint!
  } else if (event_info->eventId == TY_EVENT_LICENSE_ERROR) {
    LOGD("=== Event Callback: License Error!");
  }
}

class camport_initer {
public:
  camport_initer() {
    CHECK_OK(TYInitLib());
    cout << "init success" << endl;
    TY_VERSION_INFO ver;
    CHECK_OK(TYLibVersion(&ver));
    std::stringstream ss;
    ss << "[" << ver.major << "." << ver.minor << "." << ver.patch << "]";
    ver_ = ss.str();
  }
  ~camport_initer() { TYDeinitLib(); }
  std::string get_version() const { return ver_; }

private:
  std::string ver_;
};

camport_initer initer;


  void camport_wrapper_impl::init_device(const TY_DEVICE_BASE_INFO& selectedDev){
    auto dev_data_p = std::make_shared<dev_data>();
    
    auto& hIface = dev_data_p->hIface;
    auto& hDevice = dev_data_p->hDevice;
    assert(hIface == nullptr);
    assert(hDevice == nullptr);
    CHECK_OK(TYOpenInterface(selectedDev.iface.id, &hIface));
    CHECK_OK(TYOpenDevice(hIface, selectedDev.id, &hDevice));
    assert(dev_data_p->hIface != nullptr);
    assert(dev_data_p->hIface != nullptr);
    
    int32_t allComps;
    CHECK_OK(TYGetComponentIDs(hDevice, &allComps));
    if (allComps & TY_COMPONENT_RGB_CAM) {
      dev_data_p->rgb_support = true;
      CHECK_OK(TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM));
    }
    if (allComps & TY_COMPONENT_IR_CAM_LEFT) {
      dev_data_p->ir_left_support = true;
      CHECK_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_LEFT));
    }
    if (allComps & TY_COMPONENT_IR_CAM_RIGHT) {
      dev_data_p->ir_right_support = true;
      CHECK_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_RIGHT));
    }
    if (allComps & TY_COMPONENT_DEPTH_CAM) {
      dev_data_p->depth_support = true;
      std::vector<TY_ENUM_ENTRY> image_mode_list;
      CHECK_OK(get_feature_enum_list(hDevice, TY_COMPONENT_DEPTH_CAM,
                                     TY_ENUM_IMAGE_MODE, image_mode_list));
      for (size_t idx = 0; idx < image_mode_list.size(); idx++) {
        TY_ENUM_ENTRY &entry = image_mode_list[idx];
        // try to select a vga resolution
        dev_data_p->width = TYImageWidth(entry.value);
        dev_data_p->height = TYImageHeight(entry.value);
        cout << "depth image width:" << dev_data_p->width << " height:" << dev_data_p->height << endl;
        if (TYImageWidth(entry.value) == 640 ||
            TYImageHeight(entry.value) == 640) {
          LOGD("Select Depth Image Mode: %s", entry.description);
          int err = TYSetEnum(hDevice, TY_COMPONENT_DEPTH_CAM,
                              TY_ENUM_IMAGE_MODE, entry.value);
          assert(err == TY_STATUS_OK || err == TY_STATUS_NOT_PERMITTED);
          break;
        }
      }
      CHECK_OK(TYEnableComponents(hDevice, TY_COMPONENT_DEPTH_CAM));
    }

    LOGD("prepare image buffer");
    uint32_t frameSize;
    CHECK_OK(TYGetFrameBufferSize(hDevice, &frameSize));
    LOGD("     - Get size of framebuffer, %d", frameSize);
    assert(frameSize >= 640 * 480 * 2);

    LOGD("     - Allocate & enqueue buffers");

    dev_data_p->frameBuffer[0].reserve(frameSize);
    dev_data_p->frameBuffer[1].reserve(frameSize);
    LOGD("     - Enqueue buffer (%p, %d)", dev_data_p->frameBuffer[0].data(), frameSize);
    CHECK_OK(TYEnqueueBuffer(hDevice, dev_data_p->frameBuffer[0].data(), frameSize));
    LOGD("     - Enqueue buffer (%p, %d)", dev_data_p->frameBuffer[1].data(), frameSize);
    CHECK_OK(TYEnqueueBuffer(hDevice, dev_data_p->frameBuffer[1].data(), frameSize));

    LOGD("Register event callback");
    CHECK_OK(TYRegisterEventCallback(hDevice, eventCallback, nullptr));

    bool hasTrigger;
    CHECK_OK(TYHasFeature(hDevice, TY_COMPONENT_DEVICE, TY_STRUCT_TRIGGER_PARAM,
                          &hasTrigger));
    if (hasTrigger) {
      LOGD("Disable trigger mode");
      TY_TRIGGER_PARAM trigger;
      trigger.mode = TY_TRIGGER_MODE_OFF;
      CHECK_OK(TYSetStruct(hDevice, TY_COMPONENT_DEVICE,
                           TY_STRUCT_TRIGGER_PARAM, &trigger, sizeof(trigger)));
    }
    dev_data_p->base_info = selectedDev;
    selected_devs_[selectedDev.id] = dev_data_p;
  }

std::vector<std::shared_ptr<camport_device>> 
camport_wrapper_impl::find_devices(uint32_t device_num){
    std::vector<TY_DEVICE_BASE_INFO> selected;
    CHECK_OK(selectDevice(TY_INTERFACE_ALL, "", "", device_num, selected));
    if (selected.empty()) {
      throw std::runtime_error("has not selected device");
    }

    LOGD("selected size:%d", selected.size());
    for(const auto& selectedDev : selected){
      cout << "begin init seleted dev, id:" << selectedDev.id << endl;
      init_device(selectedDev);
    }
    std::vector<std::shared_ptr<camport_device>> devs;
    for(auto& d : selected_devs_){
      auto dev = std::make_shared<camport_device_impl>(d.second);
      devs.push_back(dev);
    }
    return devs;
}

std::shared_ptr<camport_wrapper> get_wrapper(){
  static std::shared_ptr<camport_wrapper> wrapper = std::make_shared<camport_wrapper_impl>();
  return wrapper;
}
/////////////////////////

/*

*/

} // namespace deepir::camport_wrapper
}