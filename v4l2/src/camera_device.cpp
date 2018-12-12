#include "camera_device.hpp"
#include "util.hpp"
#include <deepir/image/image.hpp>
#include <fcntl.h>
#include <iostream>
#include <linux/videodev2.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
using std::cout;
using std::endl;

namespace fs = std::filesystem;
camera_device::camera_device(const std::filesystem::path &p) : dev_path_(p) {
  fd_ = open_device(dev_path_);
  init_device(fd_);
}
camera_device::~camera_device() {
  if (fd_ == -1) {
    return;
  }
  uninit_device();
  close_device(fd_);
  stop_capturing();
  fd_ = -1;
}
void camera_device::close_device(int dev_fd) {
  if (dev_fd == -1)
    return;
  close(dev_fd);
}

int camera_device::open_device(const std::filesystem::path &dev_path) {

  // struct stat st;
  if (!fs::exists(dev_path)) {
    throw std::runtime_error(dev_path.string() + " not exists");
  }
  if (!fs::is_character_file(dev_path)) {
    throw std::runtime_error(dev_path.string() + " is not a char device");
  }
  auto device_name = dev_path.string();

  int dev_fd = open(device_name.c_str(),
                    O_RDWR /* required */ | O_NONBLOCK | O_CLOEXEC, 0);

  if (-1 == dev_fd) {
    throw std::runtime_error(device_name + " open fail");
  }
  return dev_fd;
}

void camera_device::init_device(int dev_fd) {
  struct v4l2_capability cap {};
  struct v4l2_cropcap cropcap {};
  struct v4l2_crop crop {};
  struct v4l2_format fmt {};
  unsigned int min;
  auto dev_name = dev_path_.string();
  if (-1 == xioctl(dev_fd, VIDIOC_QUERYCAP, &cap)) {
    if (EINVAL == errno) {
      throw std::runtime_error(dev_name + " is not V4L2 device" + errno_str);
    } else {
      throw std::runtime_error(dev_name + " do VIDIOC_QUERYCAP fail" +
                               errno_str);
    }
  }

  if (!(cap.capabilities & V4L2_CAP_VIDEO_CAPTURE)) {
    throw std::runtime_error(dev_name + " is not video capture device");
  }
  if (!(cap.capabilities & V4L2_CAP_STREAMING)) {
    throw std::runtime_error(dev_name + " does not support streaming i/o");
  }
  /* Select video input, video standard and tune here. */
  CLEAR(cropcap);
  cropcap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;

  if (0 == xioctl(dev_fd, VIDIOC_CROPCAP, &cropcap)) {
    crop.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    crop.c = cropcap.defrect; /* reset to default */

    if (-1 == xioctl(dev_fd, VIDIOC_S_CROP, &crop)) {
      // throw std::runtime_error(errno_str + "VIDIOC_S_CROP fail");
      cout << "!!!!!!!! ignore error" << endl;
    }
  } else {
    /* Errors ignored. */
    cout << "!!!!!!!!!!!!!!!!!warning" << endl;
  }

  CLEAR(fmt);

  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = 640;
  fmt.fmt.pix.height = 480;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_MJPEG;
  fmt.fmt.pix.field = V4L2_FIELD_INTERLACED;

  if (-1 == xioctl(dev_fd, VIDIOC_S_FMT, &fmt)) {
    throw std::runtime_error("VIDIOC_S_FMT fail");
  }
  if (-1 == xioctl(dev_fd, VIDIOC_G_FMT, &fmt)) {
    throw std::runtime_error("VIDIOC_G_FMT fail");
  }
  cout << "---format:" << endl
       << "width:" << fmt.fmt.pix.width << endl
       << "height:" << fmt.fmt.pix.height << endl;
  if (fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_MJPEG) {
    throw std::runtime_error("pixealformat is not V4L2_PIX_FMT_MJPEG");
  }

  /* Buggy driver paranoia. */
  min = fmt.fmt.pix.width * 2;
  if (fmt.fmt.pix.bytesperline < min)
    fmt.fmt.pix.bytesperline = min;
  min = fmt.fmt.pix.bytesperline * fmt.fmt.pix.height;
  if (fmt.fmt.pix.sizeimage < min)
    fmt.fmt.pix.sizeimage = min;
  cout << "init_device of fd:" << fd_ << " success" << endl;
  init_mmap();
}
void camera_device::uninit_device(void) {
  for (size_t i = 0; i < device_buffers_.size(); ++i) {
    if (-1 == munmap(device_buffers_[i].start, device_buffers_[i].length)) {
      cout << "munmap fail" << endl;
    }
  }
}

void camera_device::init_mmap(void) {
  struct v4l2_requestbuffers req;

  CLEAR(req);

  req.count = buffer_size_;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  auto dev_name = dev_path_.string();
  if (-1 == xioctl(fd_, VIDIOC_REQBUFS, &req)) {
    if (EINVAL == errno) {
      throw std::runtime_error(dev_name + " does not support memory mapping" +
                               errno_str);
    } else {
      throw std::runtime_error(dev_name + " VIDIOC_REQBUFS fail" + errno_str);
    }
  }

  if (req.count != buffer_size_) {
    throw std::runtime_error(std::string("device buffer size is ") +
                             std::to_string(req.count) + " but except " +
                             std::to_string(buffer_size_));
  }
  for (uint32_t n_buffers = 0; n_buffers < buffer_size_; ++n_buffers) {
    struct v4l2_buffer buf;

    CLEAR(buf);

    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = n_buffers;

    if (-1 == xioctl(fd_, VIDIOC_QUERYBUF, &buf)) {
      throw std::runtime_error(dev_name + " VIDIOC_QUERYBUF fail");
    }
    struct device_buffer dev_buf;
    dev_buf.length = buf.length;
    dev_buf.start = mmap(nullptr /* start anywhere */, buf.length,
                         PROT_READ | PROT_WRITE /* required */,
                         MAP_SHARED /* recommended */, fd_, buf.m.offset);

    if (MAP_FAILED == dev_buf.start) {
      throw std::runtime_error("mmap fail");
    }
    device_buffers_.push_back(dev_buf);
  }
  cout << "init_mmap of fd:" << fd_ << " success" << endl;
}

void camera_device::stop_capturing(void) {
  enum v4l2_buf_type type;
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == xioctl(fd_, VIDIOC_STREAMOFF, &type)) {
    cout << "VIDIOC_STREAMOFF fail" << endl;
  }
}

void camera_device::start_capturing(void) {
  enum v4l2_buf_type type;

  for (uint32_t i = 0; i < buffer_size_; ++i) {
    struct v4l2_buffer buf;
    CLEAR(buf);
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.index = i;

    if (-1 == xioctl(fd_, VIDIOC_QBUF, &buf)) {
      throw std::runtime_error("VIDIOC_QBUF fail");
    }
  }
  type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  if (-1 == xioctl(fd_, VIDIOC_STREAMON, &type)) {
    throw std::runtime_error(std::string("VIDIOC_STREAMON fail fd:") +
                             std::to_string(fd_));
  }
  cout << "start capture success fd:" << fd_ << endl;
}

bool camera_device::wait_fd_ready(int sec, int fd) {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd, &fds);
  struct timeval tv;
  tv.tv_sec = sec;
  tv.tv_usec = 0;
  while (true) {
    int r = select(fd + 1, &fds, nullptr, nullptr, &tv);
    if (-1 == r) {
      if (EINTR == errno) {
        continue;
      } else {
        throw std::runtime_error("select error " + errno_str);
      }
    }
    if (0 == r) {
      cout << "select timeout" << endl;
      return false;
    }
    return true;
  }
}
std::optional<camera_mat> camera_device::capture_frame() {
  if (!wait_fd_ready(2, fd_)) { // timeout
    return {};
  }

  struct v4l2_buffer buf {
    .type = V4L2_BUF_TYPE_VIDEO_CAPTURE, .memory = V4L2_MEMORY_MMAP
  };

  if (-1 == xioctl(fd_, VIDIOC_DQBUF, &buf)) {
    int e = errno;
    if (e == EAGAIN)
      return {};
    else {
      throw std::runtime_error("VIDIOC_DQBUF fail" + errno_str);
    }
  }
  assert(buf.index < buffer_size_);
  cout << "buf.bytesused=" << buf.bytesused << std::endl;
  auto mat_opt =
      deepir::image::load(device_buffers_[buf.index].start, buf.bytesused);
  if (!mat_opt) {
    cout << "to mat fail   fd:" << fd_ << endl;
    return {};
  }
  frame_index_++;
  camera_mat m{mat_opt.value(), frame_index_++};
  if (-1 == xioctl(fd_, VIDIOC_QBUF, &buf)) {
    throw std::runtime_error("VIDIOC_QBUF fail");
  }
  return m;
}
