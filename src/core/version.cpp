#include "core/version.h"

#include "version_config.h"

namespace fulcrum {

std::string_view version() noexcept { return FULCRUM_VERSION_STRING; }

}  // namespace fulcrum
