#pragma once

#include "Lvs/Engine/Core/Types.hpp"
#include "Lvs/Engine/Utils/Signal.hpp"

#include <filesystem>
#include <memory>
#include <mutex>
#include <optional>
#include <vector>

namespace Lvs::Engine::Core {

class Instance;

class ExternalMetadata final {
public:
    static ExternalMetadata& Get();

    void EnsureLoaded();
    void PollHotReload();

    void RegisterRoot(const std::shared_ptr<Instance>& root);
    void UnregisterRoot(const std::shared_ptr<Instance>& root);

    [[nodiscard]] String GetMetadataPath() const;
    [[nodiscard]] String GetLastLoadedToml() const;

    Utils::Signal<> Reloaded;

private:
    ExternalMetadata() = default;

    [[nodiscard]] String ResolveMetadataPath() const;
    [[nodiscard]] std::optional<std::filesystem::file_time_type> QueryLastModified(const String& osPath) const;

    bool LoadFromDiskLocked();
    void ApplyLocked();
    void SyncRegisteredRootsLocked();

    mutable std::mutex mutex_;
    bool loaded_{false};
    String metadataPath_;
    std::optional<std::filesystem::file_time_type> lastModified_;
    String tomlText_;
    String lastLoadedToml_;
    std::vector<std::weak_ptr<Instance>> roots_;
};

} // namespace Lvs::Engine::Core
