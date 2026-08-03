#pragma once

#include <filesystem>
#include <string_view>

namespace teya::core::assets {
std::filesystem::path path(std::string_view relativePath);
} // namespace teya::core::assets
