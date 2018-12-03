#include "../camport_wrapper.hpp"
#include <iostream>
using namespace std;

int main() {
  deepir::camport_wrapper::camport_wrapper wrapper;
  auto m = wrapper.next_depth_frame();
  cout << "get seq:" << m.seq << endl;
  return 0;
}
