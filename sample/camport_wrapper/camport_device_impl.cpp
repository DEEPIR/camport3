#include "camport_device_impl.hpp"
#include "check_ok.hpp"
#include <iostream>

using namespace std;

namespace deepir{
namespace camport_wrapper {

  void camport_device_impl::start() {
    LOGD("Start capture");
    CHECK_OK(TYStartCapture(dev_data_->hDevice));
  }
  mat camport_device_impl::next_depth_frame() {
    cout << "call next_depth frame" << endl;
    int err = TYFetchFrame(dev_data_->hDevice, &(dev_data_->frame_data), -1);
    if (err != TY_STATUS_OK){
      mat m;
      m.seq = -1;
      return m;
    }
    dev_data_->index++;
    cout << "get frame:" << dev_data_->index << endl;
    cv::Mat depth, irl, irr, color;
    parseFrame(dev_data_->frame_data, &depth, &irl, &irr, &color);
    mat m;
    if (!depth.empty()) {
      m.content = depth;
      m.seq = dev_data_->index;
    }
    LOGD("Re-enqueue buffer(%p, %d)"
                , dev_data_->frame_data.userBuffer, dev_data_->frame_data.bufferSize);
    CHECK_OK( TYEnqueueBuffer(dev_data_->hDevice, dev_data_->frame_data.userBuffer, dev_data_->frame_data.bufferSize) );
    return m;
  }

  std::string camport_device_impl::id(){
      return dev_data_->base_info.id;
  }

}
}