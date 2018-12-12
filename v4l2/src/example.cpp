/*
 *  V4L2 video capture example
 *
 *  This program can be used and distributed without restrictions.
 *
 *      This program is provided with the V4L2 API
 * see https://linuxtv.org/docs.php for more information
 */

#include <assert.h>
#include <getopt.h> /* getopt_long() */
#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "camera_device.hpp"
#include "util.hpp"
#include <chrono>
#include <deepir/util/runnable.hpp>
#include <deepir/util/thread_safe_container.hpp>
#include <deepir/util/time.hpp>
#include <errno.h>
#include <fcntl.h> /* low-level i/o */
#include <linux/videodev2.h>
#include <opencv2/opencv.hpp>
#include <optional>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

using namespace std;

using safe_queue = std::deque<std::vector<camera_mat>>;

class viewer : public deepir::runnable {
public:
  viewer(std::shared_ptr<camera_device> device) : device_(device) {}
  void run() override {
    uint64_t c = 0;
    while (!needs_stop()) {
      c++;
      auto opt = device_->capture_frame();
      if (!opt) {
        cout << "next_frame fail fd:" << device_->get_fd() << "  c:" << c
             << endl;
        continue;
      }
      auto c_mat = opt.value();

      cv::imwrite(std::to_string(device_->get_fd()) + "fd-" +
                      std::to_string(c) + ".jpg",
                  c_mat.content);
    }
  }

private:
  std::shared_ptr<camera_device> device_;
};

int main(int argc, char **argv) {
  if (argc < 2) {
    cout << "usage: " << argv[0] << "  video1 video2 ..." << endl;
    return 0;
  }
  cout << "errno:" << errno_str << endl;
  std::vector<std::string> video_paths;
  for (int i = 1; i < argc; i++) {
    std::string video_path = argv[i];
    video_paths.push_back(video_path);
  }
  std::vector<std::shared_ptr<camera_device>> devices;
  std::vector<std::shared_ptr<viewer>> viewers;
  for (auto path : video_paths) {
    std::shared_ptr<camera_device> camera =
        std::make_shared<camera_device>(path);
    devices.push_back(camera);
    int fd = camera->get_fd();
    cout << "push camera fd:" << fd << endl;
    camera->start_capturing();
    auto v = std::make_shared<viewer>(camera);
    v->start();
    viewers.push_back(v);
  }
  cout << "------------------" << endl;
  cin.get();
  return 0;
}
