#include "../../common/common.hpp"
#include "../camport_wrapper.hpp"
#include <iostream>
using namespace std;

int main() {
  auto wrapper = deepir::camport_wrapper::get_wrapper();
  auto devs = wrapper->find_devices(3);
  cout << "find devs size:" << devs.size() << endl;
  for (auto &dev : devs) {
    cout << "device id:" << dev->id() << endl;
  }
  if (devs.empty()) {
    return 0;
  }
  auto &dev = devs[1];
  dev->start();
  DepthViewer depthViewer("Depth");
  bool exit_main = false;
  while (!exit_main) {
    try {
      auto m = dev->next_depth_frame();
      cout << "get seq:" << m.seq << endl;
      if (m.content.empty()) {
        cout << "get empty mat" << endl;
      } else {
        cout << "get mat success, seq:" << m.seq << endl;
        cv::imshow("depth--", m.content);
        depthViewer.show(m.content);
        int key = cv::waitKey(1);
        switch (key & 0xff) {
        case 0xff:
          break;
        case 'q':
          exit_main = true;
          break;
        default:
          LOGD("Unmapped key %d", key);
        }
      }
    } catch (const std::exception &e) {
      cout << "next_depth_frame except:" << e.what();
      break;
    }
  }
  return 0;
}
