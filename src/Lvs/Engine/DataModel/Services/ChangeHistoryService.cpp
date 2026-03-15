#include "Lvs/Engine/DataModel/Services/ChangeHistoryService.hpp"

#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/DataModel.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Services/ServiceRegistry.hpp"

#include <stdexcept>

namespace Lvs::Engine::DataModel {

Core::ClassDescriptor& ChangeHistoryService::Descriptor() {
    static Core::ClassDescriptor descriptor("ChangeHistoryService", &Service::Descriptor());
    static const bool initialized = []() {
        Core::ClassDescriptor::RegisterClassDescriptor(&descriptor);
        ClassRegistry::RegisterClass<ChangeHistoryService>("ChangeHistoryService", "Services", "Service");
        ServiceRegistry::RegisterService<ChangeHistoryService>();
        return true;
    }();
    static_cast<void>(initialized);
    return descriptor;
}

ChangeHistoryService::ChangeHistoryService()
    : Service(Descriptor()) {
    SetServiceFlags(true, true);
    SetInsertable(false);
}

bool ChangeHistoryService::IsRecording() const {
    return recordingGroup_ != nullptr;
}

void ChangeHistoryService::BeginRecording(const QString& name) {
    if (recordingGroup_ != nullptr) {
        throw std::runtime_error("Already recording.");
    }
    recordingGroup_ = std::make_shared<Utils::CommandGroup>(name);
}

void ChangeHistoryService::FinishRecording() {
    if (recordingGroup_ == nullptr) {
        return;
    }

    auto group = recordingGroup_;
    recordingGroup_.reset();

    if (group->Commands().empty()) {
        return;
    }

    undoStack_.push_back(group);
    redoStack_.clear();
    TrimHistory();
}

void ChangeHistoryService::Record(const std::shared_ptr<Utils::Command>& command) {
    if (recordingGroup_ == nullptr) {
        command->Do();
        MarkPlaceDirty();
        return;
    }

    recordingGroup_->Add(command);
    command->Do();
    MarkPlaceDirty();
}

void ChangeHistoryService::Undo() {
    if (undoStack_.empty()) {
        return;
    }

    auto group = undoStack_.back();
    undoStack_.pop_back();
    group->Undo();
    MarkPlaceDirty();

    redoStack_.push_back(group);
    OnUndo.Fire(group->Name());
}

void ChangeHistoryService::Redo() {
    if (redoStack_.empty()) {
        return;
    }

    auto group = redoStack_.back();
    redoStack_.pop_back();
    group->Do();
    MarkPlaceDirty();

    undoStack_.push_back(group);
    OnRedo.Fire(group->Name());
}

void ChangeHistoryService::TrimHistory() {
    while (static_cast<int>(undoStack_.size()) > MAX_HISTORY) {
        undoStack_.erase(undoStack_.begin());
    }
}

void ChangeHistoryService::MarkPlaceDirty() const {
    const auto dataModel = GetDataModel();
    if (dataModel == nullptr) {
        return;
    }

    Place* ownerPlace = dataModel->GetOwnerPlace();
    if (ownerPlace != nullptr) {
        ownerPlace->MarkDirty(true);
    }
}

} // namespace Lvs::Engine::DataModel
