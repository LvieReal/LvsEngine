#pragma once

#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/Utils/Signal.hpp"

#include <memory>

namespace Lvs::Engine::DataModel {

class PlaceManager {
public:
    PlaceManager() = default;

    [[nodiscard]] std::shared_ptr<Place> GetCurrentPlace() const;

    std::shared_ptr<Place> NewPlace();
    std::shared_ptr<Place> OpenPlaceFromFile(const QString& filePath);
    void SaveCurrentPlaceToFile(const QString& filePath) const;
    void ClosePlace();

    Utils::Signal<const std::shared_ptr<Place>&> PlaceOpened;
    Utils::Signal<const std::shared_ptr<Place>&> PlaceClosed;

private:
    std::shared_ptr<Place> currentPlace_;
};

} // namespace Lvs::Engine::DataModel
