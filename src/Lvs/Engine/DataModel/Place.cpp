#include "Lvs/Engine/DataModel/Place.hpp"

#include "Lvs/Engine/DataModel/Services/ChangeHistoryService.hpp"
#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/Services/Lighting.hpp"
#include "Lvs/Engine/DataModel/Services/QualitySettings.hpp"
#include "Lvs/Engine/DataModel/Services/Selection.hpp"
#include "Lvs/Engine/DataModel/Services/ServiceRegistry.hpp"
#include "Lvs/Engine/DataModel/Services/Workspace.hpp"
#include "Lvs/Engine/IO/Providers.hpp"
#include "Lvs/Engine/Enums/EnumMetadata.hpp"
#include "Lvs/Engine/Enums/LightingComputationMode.hpp"
#include "Lvs/Engine/Enums/LightingTechnology.hpp"
#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Enums/PartShape.hpp"
#include "Lvs/Engine/Enums/PartSurfaceType.hpp"
#include "Lvs/Engine/Enums/TextureFiltering.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Objects/BasePart.hpp"
#include "Lvs/Engine/Objects/Camera.hpp"
#include "Lvs/Engine/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/Objects/Light.hpp"
#include "Lvs/Engine/Objects/Model.hpp"
#include "Lvs/Engine/Objects/MeshPart.hpp"
#include "Lvs/Engine/Objects/Part.hpp"
#include "Lvs/Engine/Objects/PostEffects.hpp"
#include "Lvs/Engine/Objects/SelectionBox.hpp"
#include "Lvs/Engine/Objects/Skybox.hpp"
#include "Lvs/Engine/Objects/Folder.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace Lvs::Engine::DataModel {

namespace {
constexpr auto FILE_FORMAT_VERSION = "1";

[[nodiscard]] Core::String GetAttr(IO::IXmlReader& reader, const Core::String& name) {
    const auto attrs = reader.Attributes();
    const auto it = attrs.find(name);
    return it == attrs.end() ? Core::String{} : it->second;
}

[[nodiscard]] std::vector<Core::String> Split(const Core::String& s, const char delim) {
    std::vector<Core::String> out;
    size_t start = 0;
    while (start <= s.size()) {
        const size_t pos = s.find(delim, start);
        if (pos == Core::String::npos) {
            out.emplace_back(s.substr(start));
            break;
        }
        out.emplace_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return out;
}

void EnsureBuiltinRegistrations() {
    static const bool initialized = []() {
        (void)DataModel::Descriptor();
        (void)Service::Descriptor();
        (void)Workspace::Descriptor();
        (void)Lighting::Descriptor();
        (void)QualitySettings::Descriptor();
        (void)Selection::Descriptor();
        (void)ChangeHistoryService::Descriptor();
        (void)Objects::BasePart::Descriptor();
        (void)Objects::Part::Descriptor();
        (void)Objects::MeshPart::Descriptor();
        (void)Objects::Camera::Descriptor();
        (void)Objects::Light::Descriptor();
        (void)Objects::DirectionalLight::Descriptor();
        (void)Objects::Skybox::Descriptor();
        (void)Objects::SelectionBox::Descriptor();
        (void)Objects::Model::Descriptor();
        (void)Objects::Folder::Descriptor();
        return true;
    }();
    static_cast<void>(initialized);
}

void EnsureWorkspaceCurrentCamera(const std::shared_ptr<Workspace>& workspace) {
    if (workspace == nullptr) {
        return;
    }

    const auto currentVar = workspace->GetProperty("CurrentCamera");
    if (currentVar.Is<Core::Variant::InstanceRef>()) {
        if (const auto locked = currentVar.Get<Core::Variant::InstanceRef>().lock()) {
            if (const auto camera = std::dynamic_pointer_cast<Objects::Camera>(locked);
                camera != nullptr && camera->GetParent() != nullptr) {
                return;
            }
        }
    }

    for (const auto& child : workspace->GetChildren()) {
        if (const auto camera = std::dynamic_pointer_cast<Objects::Camera>(child); camera != nullptr) {
            workspace->SetProperty("CurrentCamera", Core::Variant::InstanceRef{camera});
            return;
        }
    }

    auto camera = std::make_shared<Objects::Camera>();
    camera->SetParent(workspace);
    workspace->SetProperty("CurrentCamera", Core::Variant::InstanceRef{camera});
}

} // namespace

Place::Place() {
    CreateDefaultScene();
}

std::shared_ptr<Place> Place::LoadFromFile(const Core::String& filePath) {
    const auto fs = IO::Providers::GetFileSystem();
    const auto xml = IO::Providers::GetXml();
    if (!fs || !xml) {
        throw std::runtime_error("Place::LoadFromFile requires IO providers (FileSystem + Xml) to be registered.");
    }

    const auto text = fs->ReadAllText(filePath);
    if (!text) {
        throw std::runtime_error("Failed to read place file: " + filePath);
    }

    auto reader = xml->CreateReaderFromText(*text);
    if (!reader || !reader->ReadNextStartElement() || reader->Name() != "Place") {
        throw std::runtime_error("Invalid place file: expected root <Place>.");
    }
    if (reader->HasError()) {
        throw std::runtime_error("Invalid place file: " + reader->ErrorString());
    }

    auto place = std::make_shared<Place>();
    place->LoadFromXmlRoot(*reader);
    place->SetFilePath(filePath);
    place->MarkDirty(false);
    return place;
}

void Place::SaveToFile(const Core::String& filePath) {
    const auto fs = IO::Providers::GetFileSystem();
    const auto xml = IO::Providers::GetXml();
    if (!fs || !xml) {
        throw std::runtime_error("Place::SaveToFile requires IO providers (FileSystem + Xml) to be registered.");
    }

    const std::filesystem::path path(filePath);
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        const auto parentStr = parent.string();
        if (!fs->MkdirP(parentStr)) {
            throw std::runtime_error("Failed to create directory: " + parentStr);
        }
    }

    auto writer = xml->CreateWriter();
    if (!writer) {
        throw std::runtime_error("XML provider returned a null writer.");
    }

    writer->WriteStartDocument();
    writer->WriteStartElement("Place");
    writer->WriteAttribute("Version", FILE_FORMAT_VERSION);

    for (const auto& [serviceName, service] : services_) {
        if (service->IsHiddenService()) {
            continue;
        }

        writer->WriteStartElement("Service");
        writer->WriteAttribute("Name", serviceName);
        SerializeProperties(service, *writer);

        writer->WriteStartElement("Instances");
        for (const auto& child : service->GetChildren()) {
            SerializeInstanceRecursive(child, *writer);
        }
        writer->WriteEndElement(); // Instances
        writer->WriteEndElement(); // Service
    }

    writer->WriteEndElement(); // Place
    writer->WriteEndDocument();

    if (!fs->WriteAllText(filePath, writer->GetText())) {
        throw std::runtime_error("Failed to write place file: " + filePath);
    }

    SetFilePath(filePath);
    MarkDirty(false);
}

std::shared_ptr<DataModel> Place::GetDataModel() const {
    return dataModel_;
}

std::shared_ptr<Core::Instance> Place::FindInstanceById(const Core::String& instanceId) const {
    if (dataModel_ == nullptr) {
        return nullptr;
    }
    return dataModel_->FindInstanceById(instanceId);
}

std::shared_ptr<Service> Place::FindService(const Core::String& name) const {
    const auto it = services_.find(name);
    return it == services_.end() ? nullptr : it->second;
}

Core::String Place::GetFilePath() const {
    return filePath_;
}

void Place::SetFilePath(const Core::String& path) {
    filePath_ = path;
}

bool Place::IsDirty() const {
    return dirty_;
}

void Place::MarkDirty(const bool value) {
    dirty_ = value;
}

void Place::Destroy() {
    if (dataModel_ != nullptr) {
        dataModel_->Destroy();
        dataModel_.reset();
    }

    for (auto& [_, service] : services_) {
        service->Destroy();
    }
    services_.clear();
}

void Place::CreateDefaultScene() {
    EnsureBuiltinRegistrations();

    dataModel_ = std::make_shared<DataModel>();
    dataModel_->SetOwnerPlace(this);

    for (const auto& createService : ServiceRegistry::GetServiceClasses()) {
        auto service = createService();
        service->SetParent(dataModel_);
        services_.insert_or_assign(service->GetClassName(), service);

        if (const auto workspace = std::dynamic_pointer_cast<Workspace>(service); workspace != nullptr) {
            workspace->InitializeDefaultObjects();
        }
        if (const auto lighting = std::dynamic_pointer_cast<Lighting>(service); lighting != nullptr) {
            lighting->InitializeDefaultObjects();
        }
    }

    if (const auto workspaceService = std::dynamic_pointer_cast<Workspace>(FindService("Workspace"));
        workspaceService != nullptr) {
        auto basePlate = std::make_shared<Objects::Part>();
        basePlate->SetProperty("Name", "BasePlate");
        basePlate->SetProperty("Anchored", true);
        basePlate->SetProperty("Locked", true);
        basePlate->SetProperty("Size", Math::Vector3{1024.0, 6.0, 1024.0});
        basePlate->SetParent(workspaceService);
    }

    MarkDirty(false);
}

void Place::LoadFromXmlRoot(IO::IXmlReader& reader) {
    while (reader.ReadNextStartElement()) {
        if (reader.Name() != "Service") {
            reader.SkipCurrentElement();
            continue;
        }

        const Core::String serviceName = GetAttr(reader, "Name");
        auto service = FindService(serviceName);
        if (service == nullptr) {
            reader.SkipCurrentElement();
            continue;
        }

        PrepareServiceForLoad(service);

        while (reader.ReadNextStartElement()) {
            if (reader.Name() == "Properties") {
                DeserializeProperties(service, reader);
                continue;
            }
            if (reader.Name() == "Instances") {
                while (reader.ReadNextStartElement()) {
                    if (reader.Name() == "Instance") {
                        DeserializeInstanceRecursive(reader, service);
                    } else {
                        reader.SkipCurrentElement();
                    }
                }
                continue;
            }
            reader.SkipCurrentElement();
        }
    }

    EnsureWorkspaceCurrentCamera(std::dynamic_pointer_cast<Workspace>(FindService("Workspace")));

    if (const auto lighting = std::dynamic_pointer_cast<Lighting>(FindService("Lighting")); lighting != nullptr) {
        std::shared_ptr<Objects::PostEffects> postEffects{};
        for (const auto& child : lighting->GetChildren()) {
            postEffects = std::dynamic_pointer_cast<Objects::PostEffects>(child);
            if (postEffects != nullptr) {
                break;
            }
        }
        if (postEffects == nullptr) {
            postEffects = std::make_shared<Objects::PostEffects>();
            postEffects->SetParent(lighting);
        }

        auto migrate = [&](const char* name) {
            const Core::String prop(name);
            try {
                const auto& def = postEffects->GetPropertyObject(prop).Definition();
                const Core::Variant postValue = postEffects->GetProperty(prop);
                const Core::Variant legacyValue = lighting->GetProperty(prop);
                if (postValue == def.Default && legacyValue != def.Default) {
                    postEffects->SetProperty(prop, legacyValue);
                }
            } catch (...) {
                // Ignore missing properties.
            }
        };

        migrate("GammaCorrection");
        migrate("Dithering");
        migrate("NeonEnabled");
        migrate("InaccurateNeon");
        migrate("NeonBlur");
    }
}

void Place::PrepareServiceForLoad(const std::shared_ptr<Service>& service) {
    for (const auto& child : service->GetChildren()) {
        if (child->IsInsertable()) {
            child->Destroy();
        }
    }
}

void Place::SerializeInstanceRecursive(const std::shared_ptr<Core::Instance>& instance, IO::IXmlWriter& writer) const {
    if (!instance->IsInsertable()) {
        return;
    }

    writer.WriteStartElement("Instance");
    writer.WriteAttribute("Class", instance->GetClassName());
    SerializeProperties(instance, writer);

    writer.WriteStartElement("Children");
    for (const auto& child : instance->GetChildren()) {
        SerializeInstanceRecursive(child, writer);
    }
    writer.WriteEndElement(); // Children
    writer.WriteEndElement(); // Instance
}

std::shared_ptr<Core::Instance> Place::DeserializeInstanceRecursive(
    IO::IXmlReader& reader,
    const std::shared_ptr<Core::Instance>& parent
) {
    const Core::String className = GetAttr(reader, "Class");
    if (className.empty()) {
        reader.SkipCurrentElement();
        return nullptr;
    }

    auto instance = ClassRegistry::CreateInstance(className);

    while (reader.ReadNextStartElement()) {
        if (reader.Name() == "Properties") {
            DeserializeProperties(instance, reader);
            continue;
        }
        if (reader.Name() == "Children") {
            while (reader.ReadNextStartElement()) {
                if (reader.Name() == "Instance") {
                    DeserializeInstanceRecursive(reader, instance);
                } else {
                    reader.SkipCurrentElement();
                }
            }
            continue;
        }
        reader.SkipCurrentElement();
    }

    instance->SetParent(parent);
    return instance;
}

void Place::SerializeProperties(const std::shared_ptr<Core::Instance>& instance, IO::IXmlWriter& writer) const {
    writer.WriteStartElement("Properties");

    for (const auto& [_, property] : instance->GetProperties()) {
        const auto& definition = property.Definition();

        if (!definition.Serializable || definition.Name == "ClassName" || definition.IsInstanceReference) {
            continue;
        }

        const Core::String encoded = EncodeValue(property.Get());
        writer.WriteStartElement("Property");
        writer.WriteAttribute("Name", definition.Name);
        writer.WriteAttribute("Type", std::to_string(static_cast<int>(definition.Type)));
        writer.WriteCharacters(encoded);
        writer.WriteEndElement();
    }

    writer.WriteEndElement(); // Properties
}

void Place::DeserializeProperties(const std::shared_ptr<Core::Instance>& instance, IO::IXmlReader& reader) {
    while (reader.ReadNextStartElement()) {
        if (reader.Name() != "Property") {
            reader.SkipCurrentElement();
            continue;
        }

        const Core::String propertyNameStd = GetAttr(reader, "Name");
        const Core::String rawValue = reader.ReadElementText();
        if (propertyNameStd.empty() || propertyNameStd == "ClassName") {
            continue;
        }

        const auto& props = instance->GetProperties();
        const auto it = props.find(propertyNameStd);
        if (it == props.end()) {
            continue;
        }

        const auto& definition = it->second.Definition();
        if (definition.IsInstanceReference) {
            continue;
        }

        instance->SetProperty(propertyNameStd, DecodeValue(definition, rawValue));
    }
}

Core::String Place::EncodeValue(const Core::Variant& value) const {
    if (!value.IsValid() || value.IsNull()) {
        return {};
    }

    auto encodeDouble = [](const double v) -> Core::String {
        std::ostringstream oss;
        oss.setf(std::ios::fmtflags(0), std::ios::floatfield);
        oss << std::setprecision(std::numeric_limits<double>::max_digits10) << v;
        return oss.str();
    };

    switch (value.GetTypeId()) {
    case Core::TypeId::Invalid:
        return {};
    case Core::TypeId::Bool:
        return value.toBool() ? "true" : "false";
    case Core::TypeId::Int:
        return std::to_string(value.toLongLong());
    case Core::TypeId::Enum:
        return std::to_string(value.toInt());
    case Core::TypeId::Double:
        return encodeDouble(value.toDouble());
    case Core::TypeId::Vector3: {
        const auto v = value.Get<Math::Vector3>();
        return encodeDouble(v.x) + "," + encodeDouble(v.y) + "," + encodeDouble(v.z);
    }
    case Core::TypeId::Color3: {
        const auto c = value.Get<Math::Color3>();
        return encodeDouble(c.r) + "," + encodeDouble(c.g) + "," + encodeDouble(c.b);
    }
    case Core::TypeId::CFrame: {
        const auto c = value.Get<Math::CFrame>();
        const auto p = c.Position;
        const auto r = c.Right;
        const auto u = c.Up;
        const auto b = c.Back;
        return encodeDouble(p.x) + "," + encodeDouble(p.y) + "," + encodeDouble(p.z) + ";"
            + encodeDouble(r.x) + "," + encodeDouble(r.y) + "," + encodeDouble(r.z) + ";"
            + encodeDouble(u.x) + "," + encodeDouble(u.y) + "," + encodeDouble(u.z) + ";"
            + encodeDouble(b.x) + "," + encodeDouble(b.y) + "," + encodeDouble(b.z);
    }
    case Core::TypeId::String:
        return value.Get<Core::String>();
    case Core::TypeId::InstanceRef:
    case Core::TypeId::ByteArray:
        return {};
    }

    return value.toString();
}

Core::Variant Place::DecodeValue(const Core::PropertyDefinition& definition, const Core::String& rawValue) const {
    const Core::TypeId typeId = definition.Type;

    if (typeId == Core::TypeId::String) {
        return Core::Variant::From(rawValue);
    }
    if (typeId == Core::TypeId::Vector3) {
        const auto parts = Split(rawValue, ',');
        if (parts.size() != 3U) {
            throw std::runtime_error("Invalid Vector3 serialized value.");
        }
        return Core::Variant::From(Math::Vector3{
            std::stod(parts[0]),
            std::stod(parts[1]),
            std::stod(parts[2])
        });
    }
    if (typeId == Core::TypeId::Color3) {
        const auto parts = Split(rawValue, ',');
        if (parts.size() != 3U) {
            throw std::runtime_error("Invalid Color3 serialized value.");
        }
        return Core::Variant::From(Math::Color3{
            std::stod(parts[0]),
            std::stod(parts[1]),
            std::stod(parts[2])
        });
    }
    if (typeId == Core::TypeId::CFrame) {
        const auto segs = Split(rawValue, ';');
        if (segs.size() != 4U) {
            throw std::runtime_error("Invalid CFrame serialized value.");
        }
        auto parseVec = [](const Core::String& part) -> Math::Vector3 {
            const auto vals = Split(part, ',');
            if (vals.size() != 3U) {
                throw std::runtime_error("Invalid CFrame vector segment.");
            }
            return {std::stod(vals[0]), std::stod(vals[1]), std::stod(vals[2])};
        };
        return Core::Variant::From(Math::CFrame{
            parseVec(segs[0]),
            parseVec(segs[1]),
            parseVec(segs[2]),
            parseVec(segs[3])
        });
    }

    Core::Variant value = Core::Variant::From(rawValue);
    value.Convert(typeId);
    return value;
}

} // namespace Lvs::Engine::DataModel
