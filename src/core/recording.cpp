#include "stickyaim/core/recording.hpp"

#include <cstdio>
#include <sstream>

#include "stickyaim/core/config_io.hpp"

namespace sa {

bool saveRecording(const std::vector<RawSample>& samples, const std::string& path, std::string* error) {
    std::string out;
    out.reserve(samples.size() * 48 + 64);
    out += "t_ns,sequence,buttons,lt,rt,lx,ly,rx,ry\n";
    char line[160];
    for (const auto& s : samples) {
        std::snprintf(line, sizeof(line), "%lld,%u,%u,%u,%u,%d,%d,%d,%d\n", static_cast<long long>(s.t), s.sequence,
                      s.pad.buttons, s.pad.leftTrigger, s.pad.rightTrigger, s.pad.lx, s.pad.ly, s.pad.rx, s.pad.ry);
        out += line;
    }
    return writeTextFile(path, out, error);
}

bool loadRecording(const std::string& path, std::vector<RawSample>& out, std::string* error) {
    std::string text;
    if (!readTextFile(path, text, error)) return false;
    std::istringstream in(text);
    std::string line;
    out.clear();
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        if (lineNo == 1 && line.rfind("t_ns", 0) == 0) continue;
        if (line.empty() || line[0] == '#') continue;
        long long t = 0;
        unsigned seq = 0, buttons = 0, lt = 0, rt = 0;
        int lx = 0, ly = 0, rx = 0, ry = 0;
        if (std::sscanf(line.c_str(), "%lld,%u,%u,%u,%u,%d,%d,%d,%d", &t, &seq, &buttons, &lt, &rt, &lx, &ly, &rx, &ry) != 9) {
            if (error) *error = "malformed line " + std::to_string(lineNo) + " in " + path;
            return false;
        }
        RawSample s;
        s.t = t;
        s.sequence = seq;
        s.pad.buttons = static_cast<std::uint16_t>(buttons);
        s.pad.leftTrigger = static_cast<std::uint8_t>(lt);
        s.pad.rightTrigger = static_cast<std::uint8_t>(rt);
        s.pad.lx = static_cast<std::int16_t>(lx);
        s.pad.ly = static_cast<std::int16_t>(ly);
        s.pad.rx = static_cast<std::int16_t>(rx);
        s.pad.ry = static_cast<std::int16_t>(ry);
        out.push_back(s);
    }
    return true;
}

}  // namespace sa
