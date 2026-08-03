#pragma once

#include <filesystem>
#include <nlohmann/json_fwd.hpp>

namespace teya::core::json_file {
nlohmann::json load(const std::filesystem::path& path);
void save(const std::filesystem::path& path, const nlohmann::json& document);
} // namespace teya::core::json_file
