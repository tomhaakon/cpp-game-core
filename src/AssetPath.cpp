#include <teya/core/AssetPath.h>
#include <string>

namespace teya::core::assets
{

    std::filesystem::path path(std::string_view relativePath)
    {
#ifdef TEYA_DEVELOPMENT_ASSET_DIR
        const std::filesystem::path assetRoot{TEYA_DEVELOPMENT_ASSET_DIR};
#else
        const std::filesystem::path assetRoot{"assets"};
#endif
        return assetRoot / std::filesystem::path{std::string{relativePath}};
    }

} // namespace teya::core::assets
