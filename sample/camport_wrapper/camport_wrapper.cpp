#include "camport_wrapper.hpp"
#include "../common/common.hpp"
#include "check_ok.hpp"
#include <iostream>
#include <sstream>
using std::cout;
using std::endl;
namespace deepir::camport_wrapper {

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
class camport_wrapper::impl {
public:
  impl(camport_wrapper *outter) : outter_(outter) {
    cout << "camport lib version:" << initer.get_version() << endl;
  }
  void init() {
    std::vector<TY_DEVICE_BASE_INFO> selected;
    CHECK_OK(selectDevice(TY_INTERFACE_ALL, ID_, IP_, 1, selected));
    if (selected.empty()) {
      throw std::runtime_error("has not selected device");
    }

    TY_DEVICE_BASE_INFO &selectedDev = selected[0];

    CHECK_OK(TYOpenInterface(selectedDev.iface.id, &hIface));
    CHECK_OK(TYOpenDevice(hIface, selectedDev.id, &hDevice));

    int32_t allComps;
    CHECK_OK(TYGetComponentIDs(hDevice, &allComps));
    if (allComps & TY_COMPONENT_RGB_CAM) {
      rgb_support = true;
      CHECK_OK(TYEnableComponents(hDevice, TY_COMPONENT_RGB_CAM));
    }
    if (allComps & TY_COMPONENT_IR_CAM_LEFT) {
      ir_left_support = true;
      CHECK_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_LEFT));
    }
    if (allComps & TY_COMPONENT_IR_CAM_RIGHT) {
      ir_right_support = true;
      CHECK_OK(TYEnableComponents(hDevice, TY_COMPONENT_IR_CAM_RIGHT));
    }
    if (allComps & TY_COMPONENT_DEPTH_CAM) {
      depth_support = true;
      std::vector<TY_ENUM_ENTRY> image_mode_list;
      CHECK_OK(get_feature_enum_list(hDevice, TY_COMPONENT_DEPTH_CAM,
                                     TY_ENUM_IMAGE_MODE, image_mode_list));
      for (size_t idx = 0; idx < image_mode_list.size(); idx++) {
        TY_ENUM_ENTRY &entry = image_mode_list[idx];
        // try to select a vga resolution
        width = TYImageWidth(entry.value);
        height = TYImageHeight(entry.value);
        cout << "depth image width:" << width << " height:" << height << endl;
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

    frameBuffer_[0].reserve(frameSize);
    frameBuffer_[1].reserve(frameSize);
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer_[0].data(), frameSize);
    CHECK_OK(TYEnqueueBuffer(hDevice, frameBuffer_[0].data(), frameSize));
    LOGD("     - Enqueue buffer (%p, %d)", frameBuffer_[1].data(), frameSize);
    CHECK_OK(TYEnqueueBuffer(hDevice, frameBuffer_[1].data(), frameSize));

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
  }
  void start() {
    LOGD("Start capture");
    CHECK_OK(TYStartCapture(hDevice));
  }
  mat next_depth_frame() {
    cout << "call next_depth frame" << endl;
    int err = TYFetchFrame(hDevice, &frame_data_, -1);
    CHECK_OK(err);
    index_++;
    cout << "get frame:" << index_ << endl;
    cv::Mat depth, irl, irr, color;
    parseFrame(frame_data_, &depth, &irl, &irr, &color);
    mat m;
    if (!depth.empty()) {
      m.content = depth;
      m.seq = index_;
    }
    return m;
  }
  ~impl() {
    if (hDevice) {
      TYStopCapture(hDevice);
      TYCloseDevice(hDevice);
    }
    if (hIface) {
      TYCloseInterface(hIface);
    }
  }

private:
  camport_wrapper *outter_;
  TY_INTERFACE_HANDLE hIface = nullptr;
  TY_DEV_HANDLE hDevice = nullptr;
  std::string ID_, IP_;
  bool rgb_support{false};
  bool ir_left_support{false};
  bool ir_right_support{false};
  bool depth_support{false};
  int width{0};
  int height{0};
  std::vector<std::vector<char>> frameBuffer_{2};
  TY_FRAME_DATA frame_data_;
  uint64_t index_{0};
};

camport_wrapper::camport_wrapper() { pimpl = std::make_unique<impl>(this); }
camport_wrapper::~camport_wrapper() = default;
mat camport_wrapper::next_depth_frame() { return pimpl->next_depth_frame(); }
void camport_wrapper::init() { pimpl->init(); }
void camport_wrapper::start() { pimpl->start(); }

} // namespace deepir::camport_wrapper
