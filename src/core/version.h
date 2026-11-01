#pragma once

#include <string_view>

namespace fulcrum {

// Version from CMake's project(VERSION ...).
std::string_view version() noexcept;

}  // namespace fulcrum
