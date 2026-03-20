#pragma once

#include "Lvs/Engine/DataModel/Services/Service.hpp"
#include "Lvs/Engine/Core/Types.hpp"
#include "Lvs/Engine/Utils/CommandGroup.hpp"
#include "Lvs/Engine/Utils/Signal.hpp"

#include <memory>
#include <vector>

namespace Lvs::Engine::DataModel {

class ChangeHistoryService : public Service {
public:
    static constexpr int MAX_HISTORY = 100;

    ChangeHistoryService();
    ~ChangeHistoryService() override = default;

    static Core::ClassDescriptor& Descriptor();

    [[nodiscard]] bool IsRecording() const;
    void BeginRecording(const Core::String& name = {});
    void FinishRecording();
    void Record(const std::shared_ptr<Utils::Command>& command);
    void Undo();
    void Redo();

    Utils::Signal<const Core::String&> OnUndo;
    Utils::Signal<const Core::String&> OnRedo;

private:
    void TrimHistory();
    void MarkPlaceDirty() const;

    std::vector<std::shared_ptr<Utils::CommandGroup>> undoStack_;
    std::vector<std::shared_ptr<Utils::CommandGroup>> redoStack_;
    std::shared_ptr<Utils::CommandGroup> recordingGroup_;
};

} // namespace Lvs::Engine::DataModel
