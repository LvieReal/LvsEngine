#include "Lvs/Engine/Enums/EnumMetadata.hpp"

#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Enums/LightingTechnology.hpp"
#include "Lvs/Engine/Enums/MSAA.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Enums/PartSurfaceType.hpp"
#include "Lvs/Engine/Enums/SkyboxTextureLayout.hpp"
#include "Lvs/Engine/Enums/TextureFiltering.hpp"

#include <QMetaType>

namespace Lvs::Engine::Enums::Metadata {

QList<EnumOption> OptionsForType(const int typeId) {
    using namespace Lvs::Engine::Enums;

    if (typeId == QMetaType::fromType<LightingTechnology>().id()) {
        return {{"Default", static_cast<int>(LightingTechnology::Default)}};
    }
    if (typeId == QMetaType::fromType<LightingComputationMode>().id()) {
        return {
            {"Per-Pixel", static_cast<int>(LightingComputationMode::PerPixel)},
            {"Per-Vertex", static_cast<int>(LightingComputationMode::PerVertex)}
        };
    }
    if (typeId == QMetaType::fromType<TextureFiltering>().id()) {
        return {{"Linear", static_cast<int>(TextureFiltering::Linear)}, {"Nearest", static_cast<int>(TextureFiltering::Nearest)}};
    }
    if (typeId == QMetaType::fromType<MSAA>().id()) {
        return {
            {"Off", static_cast<int>(MSAA::Off)},
            {"2x", static_cast<int>(MSAA::X2)},
            {"4x", static_cast<int>(MSAA::X4)},
            {"8x", static_cast<int>(MSAA::X8)}
        };
    }
    if (typeId == QMetaType::fromType<SkyboxTextureLayout>().id()) {
        return {
            {"Individual", static_cast<int>(SkyboxTextureLayout::Individual)},
            {"Cross", static_cast<int>(SkyboxTextureLayout::Cross)}
        };
    }
    if (typeId == QMetaType::fromType<MeshCullMode>().id()) {
        return {
            {"NoCull", static_cast<int>(MeshCullMode::NoCull)},
            {"Back", static_cast<int>(MeshCullMode::Back)},
            {"Front", static_cast<int>(MeshCullMode::Front)}
        };
    }
    if (typeId == QMetaType::fromType<PartShape>().id()) {
        return {
            {"Cube", static_cast<int>(PartShape::Cube)},
            {"Sphere", static_cast<int>(PartShape::Sphere)},
            {"Cylinder", static_cast<int>(PartShape::Cylinder)},
            {"Cone", static_cast<int>(PartShape::Cone)}
        };
    }
    if (typeId == QMetaType::fromType<PartSurfaceType>().id()) {
        return {
            {"Smooth", static_cast<int>(PartSurfaceType::Smooth)},
            {"Studs", static_cast<int>(PartSurfaceType::Studs)},
            {"Inlets", static_cast<int>(PartSurfaceType::Inlets)}
        };
    }
    return {};
}

QVariant VariantFromInt(const int typeId, const int value) {
    using namespace Lvs::Engine::Enums;

    if (typeId == QMetaType::fromType<LightingTechnology>().id()) {
        return QVariant::fromValue(static_cast<LightingTechnology>(value));
    }
    if (typeId == QMetaType::fromType<LightingComputationMode>().id()) {
        return QVariant::fromValue(static_cast<LightingComputationMode>(value));
    }
    if (typeId == QMetaType::fromType<TextureFiltering>().id()) {
        return QVariant::fromValue(static_cast<TextureFiltering>(value));
    }
    if (typeId == QMetaType::fromType<MSAA>().id()) {
        return QVariant::fromValue(static_cast<MSAA>(value));
    }
    if (typeId == QMetaType::fromType<SkyboxTextureLayout>().id()) {
        return QVariant::fromValue(static_cast<SkyboxTextureLayout>(value));
    }
    if (typeId == QMetaType::fromType<MeshCullMode>().id()) {
        return QVariant::fromValue(static_cast<MeshCullMode>(value));
    }
    if (typeId == QMetaType::fromType<PartShape>().id()) {
        return QVariant::fromValue(static_cast<PartShape>(value));
    }
    if (typeId == QMetaType::fromType<PartSurfaceType>().id()) {
        return QVariant::fromValue(static_cast<PartSurfaceType>(value));
    }
    return QVariant(value);
}

int IntFromVariant(const QVariant& value) {
    using namespace Lvs::Engine::Enums;

    if (value.typeId() == QMetaType::fromType<LightingTechnology>().id()) {
        return static_cast<int>(value.value<LightingTechnology>());
    }
    if (value.typeId() == QMetaType::fromType<LightingComputationMode>().id()) {
        return static_cast<int>(value.value<LightingComputationMode>());
    }
    if (value.typeId() == QMetaType::fromType<TextureFiltering>().id()) {
        return static_cast<int>(value.value<TextureFiltering>());
    }
    if (value.typeId() == QMetaType::fromType<MSAA>().id()) {
        return static_cast<int>(value.value<MSAA>());
    }
    if (value.typeId() == QMetaType::fromType<SkyboxTextureLayout>().id()) {
        return static_cast<int>(value.value<SkyboxTextureLayout>());
    }
    if (value.typeId() == QMetaType::fromType<MeshCullMode>().id()) {
        return static_cast<int>(value.value<MeshCullMode>());
    }
    if (value.typeId() == QMetaType::fromType<PartShape>().id()) {
        return static_cast<int>(value.value<PartShape>());
    }
    if (value.typeId() == QMetaType::fromType<PartSurfaceType>().id()) {
        return static_cast<int>(value.value<PartSurfaceType>());
    }
    return value.toInt();
}

bool IsRegisteredEnumType(const int typeId) {
    return !OptionsForType(typeId).isEmpty();
}

} // namespace Lvs::Engine::Enums::Metadata
