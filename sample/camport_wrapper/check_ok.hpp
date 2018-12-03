#pragma once
#include <stdexcept>
#ifndef CHECK_OK
#define CHECK_OK(x)                                                            \
  do {                                                                         \
    int err = (x);                                                             \
    if (err != TY_STATUS_OK) {                                                 \
      LOGE("CHECK failed: error %d(%s) at %s:%d", err, TYErrorString(err),     \
           __FILE__, __LINE__);                                                \
      throw std::runtime_error("CHECK TY_STATUS_OK fail");                     \
    }                                                                          \
  } while (0)
#endif
