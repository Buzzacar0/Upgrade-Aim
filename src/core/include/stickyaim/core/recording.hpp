#pragma once

#include <string>
#include <vector>

#include "stickyaim/core/types.hpp"

namespace sa {

// CSV recording of raw samples:
//   t_ns,sequence,buttons,lt,rt,lx,ly,rx,ry
// Timestamps are monotonic host time; only differences are meaningful.
bool saveRecording(const std::vector<RawSample>& samples, const std::string& path, std::string* error = nullptr);
bool loadRecording(const std::string& path, std::vector<RawSample>& out, std::string* error = nullptr);

}  // namespace sa
