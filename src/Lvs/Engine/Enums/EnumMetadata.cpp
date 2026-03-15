#include "Lvs/Engine/Enums/EnumMetadata.hpp"

#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Enums/LightingTechnology.hpp"
#include "Lvs/Engine/Enums/MSAA.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Enums/PartSurfaceType.hpp"
#include "Lvs/Engine/Enums/SkyboxTextureLayout.hpp"
#include "Lvs/Engine/Enums/SurfaceMipmapping.hpp"
#include "Lvs/Engine/Enums/TextureFiltering.hpp"
#include "Lvs/Engine/Enums/Theme.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"

#include <QHash>
#include <QMetaType>

#include <optional>

namespace Lvs::Engine::Enums::Metadata {

namespace {

struct EnumInfo {
    int TypeId{};
    QList<EnumOption> Options{};
    QVariant (*FromInt)(int){nullptr};
    std::optional<int> (*TryToInt)(const QVariant&){nullptr};
};

template <typename T>
QVariant EnumFromInt(const int value) {
    return QVariant::fromValue(static_cast<T>(value));
}

template <typename T>
std::optional<int> EnumTryToInt(const QVariant& value) {
    const int typeId = QMetaType::fromType<T>().id();
    if (value.typeId() == typeId) {
        return static_cast<int>(value.value<T>());
    }
    if (value.typeId() == QMetaType::Int || value.canConvert<int>()) {
        return value.toInt();
    }
    if (value.typeId() == QMetaType::QString) {
        bool ok = false;
        const QString text = value.toString().trimmed();
        const int parsed = text.toInt(&ok);
        if (ok) {
            return parsed;
        }
    }
    return std::nullopt;
}

QList<EnumOption> MakeOptions(std::initializer_list<EnumOption> options) {
    QList<EnumOption> list;
    list.reserve(static_cast<int>(options.size()));
    for (const auto& opt : options) {
        list.push_back(opt);
    }
    return list;
}

template <typename T>
EnumInfo MakeEnumInfo(std::initializer_list<EnumOption> options) {
    EnumInfo info;
    info.TypeId = QMetaType::fromType<T>().id();
    info.Options = MakeOptions(options);
    info.FromInt = &EnumFromInt<T>;
    info.TryToInt = &EnumTryToInt<T>;
    return info;
}

const QHash<int, EnumInfo>& Registry() {
    using namespace Lvs::Engine::Enums;
    using Lvs::Engine::Rendering::RenderApi;

    static const QHash<int, EnumInfo> registry = [] {
        QHash<int, EnumInfo> out;
        const auto add = [&out](const EnumInfo& info) { out.insert(info.TypeId, info); };

        add(MakeEnumInfo<LightingTechnology>({
            {"Default", static_cast<int>(LightingTechnology::Default)},
        }));
        add(MakeEnumInfo<LightingComputationMode>({
            {"Per-Pixel", static_cast<int>(LightingComputationMode::PerPixel)},
            {"Per-Vertex", static_cast<int>(LightingComputationMode::PerVertex)},
        }));
        add(MakeEnumInfo<TextureFiltering>({
            {"Linear", static_cast<int>(TextureFiltering::Linear)},
            {"Nearest", static_cast<int>(TextureFiltering::Nearest)},
        }));
        add(MakeEnumInfo<MSAA>({
            {"Off", static_cast<int>(MSAA::Off)},
            {"2x", static_cast<int>(MSAA::X2)},
            {"4x", static_cast<int>(MSAA::X4)},
            {"8x", static_cast<int>(MSAA::X8)},
        }));
        add(MakeEnumInfo<SurfaceMipmapping>({
            {"Off", static_cast<int>(SurfaceMipmapping::Off)},
            {"On", static_cast<int>(SurfaceMipmapping::On)},
        }));
        add(MakeEnumInfo<SkyboxTextureLayout>({
            {"Individual", static_cast<int>(SkyboxTextureLayout::Individual)},
            {"Cross", static_cast<int>(SkyboxTextureLayout::Cross)},
        }));
        add(MakeEnumInfo<MeshCullMode>({
            {"NoCull", static_cast<int>(MeshCullMode::NoCull)},
            {"Back", static_cast<int>(MeshCullMode::Back)},
            {"Front", static_cast<int>(MeshCullMode::Front)},
        }));
        add(MakeEnumInfo<PartShape>({
            {"Cube", static_cast<int>(PartShape::Cube)},
            {"Sphere", static_cast<int>(PartShape::Sphere)},
            {"Cylinder", static_cast<int>(PartShape::Cylinder)},
            {"Cone", static_cast<int>(PartShape::Cone)},
        }));
        add(MakeEnumInfo<PartSurfaceType>({
            {"Smooth", static_cast<int>(PartSurfaceType::Smooth)},
            {"Studs", static_cast<int>(PartSurfaceType::Studs)},
            {"Inlets", static_cast<int>(PartSurfaceType::Inlets)},
        }));
        add(MakeEnumInfo<Theme>({
            {"Light", static_cast<int>(Theme::Light)},
            {"Dark", static_cast<int>(Theme::Dark)},
        }));
        add(MakeEnumInfo<RenderApi>({
            {"Auto", static_cast<int>(RenderApi::Auto)},
            {"Vulkan", static_cast<int>(RenderApi::Vulkan)},
            {"OpenGL", static_cast<int>(RenderApi::OpenGL)},
        }));

        return out;
    }();

    return registry;
}

const EnumInfo* Find(const int typeId) {
    const auto& registry = Registry();
    const auto it = registry.find(typeId);
    return it == registry.end() ? nullptr : &it.value();
}

bool IsAllowedValue(const EnumInfo& info, const int value) {
    for (const auto& opt : info.Options) {
        if (opt.Value == value) {
            return true;
        }
    }
    return false;
}

std::optional<int> TryParseOptionName(const EnumInfo& info, const QString& nameOrNumber) {
    const QString needle = nameOrNumber.trimmed();
    if (needle.isEmpty()) {
        return std::nullopt;
    }

    for (const auto& opt : info.Options) {
        if (needle.compare(opt.Name, Qt::CaseInsensitive) == 0) {
            return opt.Value;
        }
    }

    bool ok = false;
    const int parsed = needle.toInt(&ok);
    if (ok) {
        return parsed;
    }
    return std::nullopt;
}

} // namespace

QList<EnumOption> OptionsForType(const int typeId) {
    if (const EnumInfo* info = Find(typeId); info != nullptr) {
        return info->Options;
    }
    return {};
}

QVariant VariantFromInt(const int typeId, const int value) {
    if (const EnumInfo* info = Find(typeId); info != nullptr && info->FromInt != nullptr) {
        if (IsAllowedValue(*info, value)) {
            return info->FromInt(value);
        }
        if (!info->Options.isEmpty()) {
            return info->FromInt(info->Options.front().Value);
        }
    }
    return QVariant(value);
}

int IntFromVariant(const QVariant& value) {
    if (const EnumInfo* info = Find(value.typeId()); info != nullptr && info->TryToInt != nullptr) {
        if (const auto parsed = info->TryToInt(value); parsed.has_value()) {
            return *parsed;
        }
    }
    return value.toInt();
}

QString NameFromInt(const int typeId, const int value) {
    if (const EnumInfo* info = Find(typeId); info != nullptr) {
        for (const auto& opt : info->Options) {
            if (opt.Value == value) {
                return QString::fromUtf8(opt.Name);
            }
        }
    }
    return {};
}

QString NameFromVariant(const QVariant& value) {
    const int typeId = value.typeId();
    if (!IsRegisteredEnumType(typeId)) {
        return {};
    }
    return NameFromInt(typeId, IntFromVariant(value));
}

QVariant VariantFromName(const int typeId, const QString& nameOrNumber) {
    if (const EnumInfo* info = Find(typeId); info != nullptr) {
        if (const auto parsed = TryParseOptionName(*info, nameOrNumber); parsed.has_value()) {
            return VariantFromInt(typeId, *parsed);
        }
    }
    return {};
}

QVariant CoerceVariant(const int typeId, const QVariant& value) {
    if (value.isValid() && value.typeId() == typeId) {
        return value;
    }

    if (const EnumInfo* info = Find(typeId); info != nullptr) {
        if (value.typeId() == QMetaType::QString) {
            if (const auto parsed = TryParseOptionName(*info, value.toString()); parsed.has_value()) {
                return VariantFromInt(typeId, *parsed);
            }
            return {};
        }

        if (value.typeId() == QMetaType::Int || value.typeId() == QMetaType::Double || value.canConvert<int>()) {
            return VariantFromInt(typeId, value.toInt());
        }
    }

    return {};
}

bool IsRegisteredEnumType(const int typeId) {
    return Find(typeId) != nullptr;
}

} // namespace Lvs::Engine::Enums::Metadata

