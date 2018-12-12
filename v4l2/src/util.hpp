#pragma once
#include <deepir/util/error.hpp>
#include <error.h>
#include <sys/ioctl.h>
inline int xioctl(int fh, int request, void *arg) {
  int r = -1;
  do {
    r = ioctl(fh, request, arg);
  } while (-1 == r && EINTR == errno);
  return r;
}

#define errno_str (std::string("[") + deepir::util::errno_to_str(errno) + "]")
