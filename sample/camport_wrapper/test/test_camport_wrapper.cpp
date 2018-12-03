#include "../camport_wrapper.hpp"
#include "../../common/common.hpp"
#include <iostream>
using namespace std;

int main() {
  deepir::camport_wrapper::camport_wrapper wrapper;
  
  wrapper.init();
  wrapper.start();

  DepthViewer depthViewer("Depth");
  while (true) {
    try {
      auto m = wrapper.next_depth_frame();
      cout << "get seq:" << m.seq << endl;
      if(m.content.empty()){
        cout << "get empty mat" << endl;
      }else{
        cout << "get mat success, seq:" << m.seq << endl;
        cv::imshow("depth--", m.content);
        depthViewer.show(m.content);
        int key = cv::waitKey(1);
      }
    } catch (const std::exception &e) {
      cout << "next_depth_frame except:" << e.what();
      break;
    }
  }
  return 0;
}
