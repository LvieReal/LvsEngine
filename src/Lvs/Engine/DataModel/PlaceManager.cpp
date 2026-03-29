#include "Lvs/Engine/DataModel/PlaceManager.hpp"

#include <stdexcept>

namespace Lvs::Engine::DataModel {

std::shared_ptr<Place> PlaceManager::GetCurrentPlace() const {
    return currentPlace_;
}

std::shared_ptr<Place> PlaceManager::NewPlace() {
    if (currentPlace_ != nullptr) {
        ClosePlace();
    }

    currentPlace_ = std::make_shared<Place>();
    PlaceOpened.Fire(currentPlace_);
    return currentPlace_;
}

std::shared_ptr<Place> PlaceManager::OpenPlaceFromFile(const Core::String& filePath) {
    auto place = Place::LoadFromFile(filePath);
    if (currentPlace_ != nullptr) {
        ClosePlace();
    }
    currentPlace_ = std::move(place);
    PlaceOpened.Fire(currentPlace_);
    return currentPlace_;
}

void PlaceManager::SaveCurrentPlaceToFile(const Core::String& filePath) const {
    if (currentPlace_ == nullptr) {
        throw std::runtime_error("No active place to save.");
    }
    currentPlace_->SaveToFile(filePath);
}

void PlaceManager::SaveCurrentPlaceToFileAs(const Core::String& filePath, const Place::FileFormat format) const {
    if (currentPlace_ == nullptr) {
        throw std::runtime_error("No active place to save.");
    }
    currentPlace_->SaveToFileAs(filePath, format);
}

void PlaceManager::ClosePlace() {
    if (currentPlace_ == nullptr) {
        return;
    }
    auto closed = currentPlace_;
    currentPlace_.reset();
    PlaceClosed.Fire(closed);
}

} // namespace Lvs::Engine::DataModel
