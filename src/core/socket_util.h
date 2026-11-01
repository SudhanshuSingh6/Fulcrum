#pragma once

#include <cstdint>

#include "core/fd.h"

namespace fulcrum {

// Port a socket is bound to (getsockname), or 0 on error.
std::uint16_t local_port(const Fd& fd);

}  // namespace fulcrum
