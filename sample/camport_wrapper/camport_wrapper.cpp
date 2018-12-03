#include "camport_wrapper.hpp"
#include "../common/common.hpp"
#include "check_ok.hpp"
#include <iostream>
#include <sstream>
using std::cout;
using std::endl;
namespace deepir::camport_wrapper {
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
    std::vector<TY_DEVICE_BASE_INFO> selected;
    CHECK_OK(selectDevice(TY_INTERFACE_ALL, ID_, IP_, 1, selected));
    if (selected.empty()) {
      throw std::runtime_error("has not selected device");
    }
  }
  std::string get_version() const { return ver_; }

private:
  std::string ver_;
  std::string ID_, IP_;
};

class camport_wrapper::impl {
public:
  impl(camport_wrapper *outter) : outter_(outter) {
    static camport_initer initer;
    cout << "camport lib version:" << initer.get_version() << endl;
  }
  mat next_depth_frame() {
    cout << "call next_depth frame" << endl;
    mat m;
    return m;
  }

private:
  camport_wrapper *outter_;
};

camport_wrapper::camport_wrapper() { pimpl = std::make_unique<impl>(this); }
camport_wrapper::~camport_wrapper() = default;
mat camport_wrapper::next_depth_frame() { return pimpl->next_depth_frame(); }

} // namespace deepir::camport_wrapper
