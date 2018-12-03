#include "../camport_wrapper.hpp"
#include <iostream>
using namespace std;

int main() {
  deepir::camport_wrapper::camport_wrapper wrapper;
  wrapper.init();
  wrapper.start();
  while (true) {
    try {
      auto m = wrapper.next_depth_frame();
      cout << "get seq:" << m.seq << endl;
    } catch (const std::exception &e) {
      cout << "next_depth_frame except:" << e.what();
      break;
    }
  }
  return 0;
}
