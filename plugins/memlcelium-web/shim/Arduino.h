// Host stand-in for Arduino's String: just enough for the headers this build uses.
#pragma once
#include <string>

class String : public std::string {
public:
    String() = default;
    String(const char* s) : std::string(s ? s : "") {}
    String(const std::string& s) : std::string(s) {}
    explicit String(int v) : std::string(std::to_string(v)) {}
    explicit String(unsigned v) : std::string(std::to_string(v)) {}
    explicit String(long v) : std::string(std::to_string(v)) {}
    explicit String(unsigned long v) : std::string(std::to_string(v)) {}
    explicit String(float v) : std::string(std::to_string(v)) {}
    explicit String(double v) : std::string(std::to_string(v)) {}
    size_t length() const { return size(); }
};
