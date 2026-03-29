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
#include "Lvs/Engine/DataModel/Objects/BasePart.hpp"
#include "Lvs/Engine/DataModel/Objects/Camera.hpp"
#include "Lvs/Engine/DataModel/Objects/DirectionalLight.hpp"
#include "Lvs/Engine/DataModel/Objects/Light.hpp"
#include "Lvs/Engine/DataModel/Objects/Model.hpp"
#include "Lvs/Engine/DataModel/Objects/MeshPart.hpp"
#include "Lvs/Engine/DataModel/Objects/Part.hpp"
#include "Lvs/Engine/DataModel/Objects/PostEffects.hpp"
#include "Lvs/Engine/DataModel/Objects/SelectionBox.hpp"
#include "Lvs/Engine/DataModel/Objects/Skybox.hpp"
#include "Lvs/Engine/DataModel/Objects/Folder.hpp"
#include "Lvs/Engine/Reflection/ReflectionSystem.hpp"
#include "Lvs/Engine/Utils/Toml.hpp"

#include <array>
#include <cctype>
#include <cstdint>
#include <filesystem>
#include <functional>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string_view>
#include <variant>
#include <vector>

namespace Lvs::Engine::DataModel {

namespace {
constexpr auto FILE_FORMAT_VERSION = "1";

[[nodiscard]] std::string_view LTrim(std::string_view s) {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.remove_prefix(1);
    }
    return s;
}

[[nodiscard]] Place::FileFormat DetectFileFormat(std::string_view text) {
    const auto trimmed = LTrim(text);
    if (!trimmed.empty() && trimmed.front() == '<') {
        return Place::FileFormat::Xml;
    }
    return Place::FileFormat::Toml;
}

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

void SetTomlValue(Utils::Toml::Table& table, const Core::String& key, Utils::Toml::Value value) {
    for (auto& [k, v] : table.Values) {
        if (k == key) {
            v = std::move(value);
            return;
        }
    }
    table.Values.emplace_back(key, std::move(value));
}

[[nodiscard]] Utils::Toml::Value VariantToTomlValue(const Core::Variant& value, const Core::PropertyDefinition& definition) {
    Utils::Toml::Value out;
    if (!value.IsValid() || value.IsNull()) {
        return out;
    }

    switch (definition.Type) {
    case Core::TypeId::Bool:
        out.Storage = value.toBool();
        return out;
    case Core::TypeId::Int:
        out.Storage = static_cast<std::int64_t>(value.toLongLong());
        return out;
    case Core::TypeId::Double:
        out.Storage = value.toDouble();
        return out;
    case Core::TypeId::String:
        out.Storage = value.Get<Core::String>();
        return out;
    case Core::TypeId::Enum: {
        const auto enumTypeIt = definition.CustomAttributes.find("EnumType");
        if (enumTypeIt != definition.CustomAttributes.end() && enumTypeIt->second.Is<Core::String>()) {
            const auto name = Enums::Metadata::NameFromInt(enumTypeIt->second.Get<Core::String>(), value.toInt());
            if (!name.empty()) {
                out.Storage = name;
                return out;
            }
        }
        out.Storage = static_cast<std::int64_t>(value.toInt());
        return out;
    }
    case Core::TypeId::Vector3: {
        const auto v = value.Get<Math::Vector3>();
        Utils::Toml::Array arr;
        arr.push_back(Utils::Toml::Value{.Storage = v.x});
        arr.push_back(Utils::Toml::Value{.Storage = v.y});
        arr.push_back(Utils::Toml::Value{.Storage = v.z});
        out.Storage = std::move(arr);
        return out;
    }
    case Core::TypeId::Color3: {
        const auto c = value.Get<Math::Color3>();
        Utils::Toml::Array arr;
        arr.push_back(Utils::Toml::Value{.Storage = c.r});
        arr.push_back(Utils::Toml::Value{.Storage = c.g});
        arr.push_back(Utils::Toml::Value{.Storage = c.b});
        out.Storage = std::move(arr);
        return out;
    }
    case Core::TypeId::CFrame: {
        const auto c = value.Get<Math::CFrame>();
        if (c == Math::CFrame::Identity()) {
            out.Storage = Core::String("identity");
            return out;
        }

        auto vec3Arr = [](const Math::Vector3& v3) -> Utils::Toml::Value {
            Utils::Toml::Array a;
            a.push_back(Utils::Toml::Value{.Storage = v3.x});
            a.push_back(Utils::Toml::Value{.Storage = v3.y});
            a.push_back(Utils::Toml::Value{.Storage = v3.z});
            return Utils::Toml::Value{.Storage = std::move(a)};
        };

        Utils::Toml::Array rows;
        rows.push_back(vec3Arr(c.Position));
        rows.push_back(vec3Arr(c.Right));
        rows.push_back(vec3Arr(c.Up));
        rows.push_back(vec3Arr(c.Back));
        out.Storage = std::move(rows);
        return out;
    }
    case Core::TypeId::Invalid:
    case Core::TypeId::InstanceRef:
    case Core::TypeId::ByteArray:
        return out;
    }

    out.Storage = value.toString();
    return out;
}

[[nodiscard]] std::optional<Core::Variant> TomlValueToVariant(
    const Utils::Toml::Value& value,
    const Core::PropertyDefinition& definition
) {
    if (definition.IsInstanceReference) {
        return std::nullopt;
    }

    switch (definition.Type) {
    case Core::TypeId::Bool:
        if (value.IsBool()) return Core::Variant::From(value.AsBool());
        return std::nullopt;
    case Core::TypeId::Int:
        if (value.IsInt()) return Core::Variant::From(static_cast<std::int64_t>(value.AsInt()));
        if (value.IsDouble()) return Core::Variant::From(static_cast<std::int64_t>(value.AsDouble()));
        return std::nullopt;
    case Core::TypeId::Double:
        if (value.IsDouble() || value.IsInt()) return Core::Variant::From(value.AsDouble());
        return std::nullopt;
    case Core::TypeId::String:
        if (value.IsString()) return Core::Variant::From(value.AsString());
        return std::nullopt;
    case Core::TypeId::Enum: {
        const auto enumTypeIt = definition.CustomAttributes.find("EnumType");
        const Core::String enumType = (enumTypeIt != definition.CustomAttributes.end() && enumTypeIt->second.Is<Core::String>())
            ? enumTypeIt->second.Get<Core::String>()
            : Core::String{};

        if (value.IsString()) {
            if (!enumType.empty() && Enums::Metadata::IsRegisteredEnum(enumType)) {
                const auto v = Enums::Metadata::VariantFromName(enumType, value.AsString());
                if (v.IsValid()) {
                    return v;
                }
            }
            return Core::Variant::From(0);
        }
        if (value.IsInt()) {
            return Core::Variant::From(static_cast<int>(value.AsInt()));
        }
        if (value.IsDouble()) {
            return Core::Variant::From(static_cast<int>(value.AsDouble()));
        }
        return std::nullopt;
    }
    case Core::TypeId::Vector3: {
        if (!value.IsArray() || value.AsArray().size() != 3) return std::nullopt;
        const auto& a = value.AsArray();
        return Core::Variant::From(Math::Vector3{a[0].AsDouble(), a[1].AsDouble(), a[2].AsDouble()});
    }
    case Core::TypeId::Color3: {
        if (!value.IsArray() || value.AsArray().size() != 3) return std::nullopt;
        const auto& a = value.AsArray();
        return Core::Variant::From(Math::Color3{a[0].AsDouble(), a[1].AsDouble(), a[2].AsDouble()});
    }
    case Core::TypeId::CFrame: {
        if (value.IsString()) {
            Core::String lowered;
            lowered.reserve(value.AsString().size());
            for (const char ch : value.AsString()) {
                lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
            }
            if (lowered == "identity") {
                return Core::Variant::From(Math::CFrame::Identity());
            }
            return std::nullopt;
        }
        if (!value.IsArray()) return std::nullopt;
        const auto& rows = value.AsArray();
        if (rows.size() != 4) return std::nullopt;
        auto parseVec = [](const Utils::Toml::Value& v) -> std::optional<Math::Vector3> {
            if (!v.IsArray() || v.AsArray().size() != 3) return std::nullopt;
            const auto& a = v.AsArray();
            return Math::Vector3{a[0].AsDouble(), a[1].AsDouble(), a[2].AsDouble()};
        };
        const auto p = parseVec(rows[0]);
        const auto r = parseVec(rows[1]);
        const auto u = parseVec(rows[2]);
        const auto b = parseVec(rows[3]);
        if (!p || !r || !u || !b) return std::nullopt;
        return Core::Variant::From(Math::CFrame{*p, *r, *u, *b});
    }
    case Core::TypeId::Invalid:
    case Core::TypeId::InstanceRef:
    case Core::TypeId::ByteArray:
        return std::nullopt;
    }
    return std::nullopt;
}

} // namespace

Place::Place() {
    CreateDefaultScene();
}

std::shared_ptr<Place> Place::LoadFromFile(const Core::String& filePath) {
    const auto fs = IO::Providers::GetFileSystem();
    const auto xml = IO::Providers::GetXml();
    if (!fs) {
        throw std::runtime_error("Place::LoadFromFile requires an IO FileSystem provider to be registered.");
    }

    const auto text = fs->ReadAllText(filePath);
    if (!text) {
        throw std::runtime_error("Failed to read place file: " + filePath);
    }

    auto place = std::make_shared<Place>();
    const FileFormat format = DetectFileFormat(*text);
    place->loadedFormat_ = format;
    place->preferredSaveFormat_ = format;

    if (format == FileFormat::Xml) {
        if (!xml) {
            throw std::runtime_error("Place::LoadFromFile requires an IO Xml provider to open XML place files.");
        }
        auto reader = xml->CreateReaderFromText(*text);
        if (!reader || !reader->ReadNextStartElement() || reader->Name() != "Place") {
            throw std::runtime_error("Invalid place file: expected root <Place>.");
        }
        if (reader->HasError()) {
            throw std::runtime_error("Invalid place file: " + reader->ErrorString());
        }
        place->LoadFromXmlRoot(*reader);
    } else {
        place->LoadFromTomlText(*text);
    }
    place->SetFilePath(filePath);
    place->MarkDirty(false);
    return place;
}

void Place::SaveToFile(const Core::String& filePath) {
    const auto ext = std::filesystem::path(filePath).extension().string();
    FileFormat format = preferredSaveFormat_;
    if (!ext.empty()) {
        Core::String lower(ext.data(), ext.size());
        for (char& ch : lower) {
            ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
        }
        if (lower == ".xml") {
            format = FileFormat::Xml;
        }
    }
    SaveToFileAs(filePath, format);
}

void Place::SaveToFileAs(const Core::String& filePath, const FileFormat format) {
    const auto fs = IO::Providers::GetFileSystem();
    const auto xml = IO::Providers::GetXml();
    if (!fs) {
        throw std::runtime_error("Place::SaveToFile requires an IO FileSystem provider to be registered.");
    }

    const std::filesystem::path path(filePath);
    const auto parent = path.parent_path();
    if (!parent.empty()) {
        const auto parentStr = parent.string();
        if (!fs->MkdirP(parentStr)) {
            throw std::runtime_error("Failed to create directory: " + parentStr);
        }
    }

    if (format == FileFormat::Xml) {
        if (!xml) {
            throw std::runtime_error("Place::SaveToFile requires an IO Xml provider to save XML place files.");
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
    } else {
        Core::String toml;
        SerializeToToml(toml);
        if (!fs->WriteAllText(filePath, toml)) {
            throw std::runtime_error("Failed to write place file: " + filePath);
        }
    }

    SetFilePath(filePath);
    loadedFormat_ = format;
    preferredSaveFormat_ = format;
    MarkDirty(false);
}

Place::FileFormat Place::GetLoadedFileFormat() const {
    return loadedFormat_;
}

Place::FileFormat Place::GetPreferredSaveFormat() const {
    return preferredSaveFormat_;
}

void Place::SetPreferredSaveFormat(const FileFormat format) {
    preferredSaveFormat_ = format;
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
    Reflection::EnsureInitialized();

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
    }
}

void Place::LoadFromTomlText(std::string_view text) {
    Utils::Toml::Document doc;
    try {
        doc = Utils::Toml::Parse(text);
    } catch (const std::exception&) {
        throw std::runtime_error("Invalid place TOML: parse failed.");
    }

    auto applyProps = [&](const std::shared_ptr<Core::Instance>& instance, const Utils::Toml::Table& table) {
        if (instance == nullptr) {
            return;
        }
        for (const auto& [key, val] : table.Values) {
            if (key == "ClassName") {
                continue;
            }
            const auto& props = instance->GetProperties();
            const auto it = props.find(key);
            if (it == props.end()) {
                continue;
            }
            const auto& definition = it->second.Definition();
            if (!definition.Serializable || definition.Name == "ClassName" || definition.IsInstanceReference) {
                continue;
            }
            const auto parsed = TomlValueToVariant(val, definition);
            if (!parsed.has_value()) {
                continue;
            }
            instance->SetProperty(key, *parsed);
        }
    };

    for (const auto& [serviceKey, serviceTable] : doc.Root.Children) {
        const auto* classNameV = Utils::Toml::FindValue(serviceTable, "ClassName");
        const Core::String serviceClassName = (classNameV && classNameV->IsString()) ? classNameV->AsString() : serviceKey;

        auto service = FindService(serviceClassName);
        if (service == nullptr) {
            continue;
        }

        PrepareServiceForLoad(service);
        applyProps(service, serviceTable);

        for (const auto& [childKey, childTable] : serviceTable.Children) {
            DeserializeInstanceRecursiveToml(childTable, childKey, service);
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

void Place::DeserializeInstanceRecursiveToml(
    const Utils::Toml::Table& table,
    const Core::String& tableKeyName,
    const std::shared_ptr<Core::Instance>& parent
) {
    const auto* classNameV = Utils::Toml::FindValue(table, "ClassName");
    if (classNameV == nullptr || !classNameV->IsString()) {
        return;
    }

    const Core::String className = classNameV->AsString();
    auto instance = ClassRegistry::CreateInstance(className);
    if (instance == nullptr) {
        return;
    }

    instance->SetParent(parent);

    bool hasName = false;
    for (const auto& [k, _] : table.Values) {
        if (k == "Name") {
            hasName = true;
            break;
        }
    }
    if (!hasName) {
        const auto& props = instance->GetProperties();
        if (props.find("Name") != props.end()) {
            instance->SetProperty("Name", Core::Variant::From(tableKeyName));
        }
    }

    for (const auto& [key, val] : table.Values) {
        if (key == "ClassName") {
            continue;
        }
        const auto& props = instance->GetProperties();
        const auto it = props.find(key);
        if (it == props.end()) {
            continue;
        }
        const auto& definition = it->second.Definition();
        if (!definition.Serializable || definition.Name == "ClassName" || definition.IsInstanceReference) {
            continue;
        }
        const auto parsed = TomlValueToVariant(val, definition);
        if (!parsed.has_value()) {
            continue;
        }
        instance->SetProperty(key, *parsed);
    }

    for (const auto& [childKey, childTable] : table.Children) {
        DeserializeInstanceRecursiveToml(childTable, childKey, instance);
    }
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

void Place::SerializeToToml(Core::String& out) const {
    Utils::Toml::Document doc;
    SetTomlValue(doc.Root, "schemaVersion", Utils::Toml::Value{.Storage = static_cast<std::int64_t>(1)});

    std::vector<std::pair<Core::String, std::shared_ptr<Service>>> services;
    services.reserve(services_.size());
    for (const auto& [name, service] : services_) {
        if (service != nullptr && !service->IsHiddenService()) {
            services.emplace_back(name, service);
        }
    }
    std::sort(services.begin(), services.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    auto writeProps = [&](const std::shared_ptr<Core::Instance>& instance, Utils::Toml::Table& table) {
        if (instance == nullptr) {
            return;
        }
        for (const auto& [_, property] : instance->GetProperties()) {
            const auto& definition = property.Definition();
            if (!definition.Serializable || definition.Name == "ClassName" || definition.IsInstanceReference) {
                continue;
            }
            Utils::Toml::Value v = VariantToTomlValue(property.Get(), definition);
            if (std::holds_alternative<std::monostate>(v.Storage)) {
                continue;
            }
            SetTomlValue(table, definition.Name, std::move(v));
        }
    };

    auto makeUniqueKey = [](Core::String base, Core::HashMap<Core::String, int>& used) {
        if (base.empty()) {
            base = "Instance";
        }
        if (used.find(base) == used.end()) {
            used.insert_or_assign(base, 1);
            return base;
        }
        int& counter = used[base];
        ++counter;
        return base + "_" + std::to_string(counter);
    };

    // Serialize services + instance trees.
    for (const auto& [serviceName, service] : services) {
        std::vector<Core::String> servicePath{serviceName};
        auto& serviceTable = Utils::Toml::GetOrCreateTablePath(doc.Root, servicePath);
        SetTomlValue(serviceTable, "ClassName", Utils::Toml::Value{.Storage = service->GetClassName()});
        writeProps(service, serviceTable);

        // For each service, ensure sibling keys are unique.
        Core::HashMap<Core::String, int> siblingUsed;
        for (const auto& child : service->GetChildren()) {
            if (child == nullptr || !child->IsInsertable()) {
                continue;
            }

            Core::String childKey{};
            if (const auto nameVar = child->GetProperty("Name"); nameVar.IsValid() && nameVar.Is<Core::String>()) {
                childKey = nameVar.Get<Core::String>();
            } else {
                childKey = child->GetClassName();
            }
            childKey = makeUniqueKey(std::move(childKey), siblingUsed);

            std::vector<Core::String> childPath = servicePath;
            childPath.push_back(childKey);
            auto& childTable = Utils::Toml::GetOrCreateTablePath(doc.Root, childPath);
            SetTomlValue(childTable, "ClassName", Utils::Toml::Value{.Storage = child->GetClassName()});
            writeProps(child, childTable);

            // Recurse descendants.
            std::function<void(const std::shared_ptr<Core::Instance>&, const std::vector<Core::String>&)> writeChildren;
            writeChildren = [&](const std::shared_ptr<Core::Instance>& parentInstance, const std::vector<Core::String>& parentPath) {
                Core::HashMap<Core::String, int> used;
                for (const auto& grandChild : parentInstance->GetChildren()) {
                    if (grandChild == nullptr || !grandChild->IsInsertable()) {
                        continue;
                    }
                    Core::String key{};
                    if (const auto nameVar = grandChild->GetProperty("Name"); nameVar.IsValid() && nameVar.Is<Core::String>()) {
                        key = nameVar.Get<Core::String>();
                    } else {
                        key = grandChild->GetClassName();
                    }
                    key = makeUniqueKey(std::move(key), used);

                    std::vector<Core::String> p = parentPath;
                    p.push_back(key);
                    auto& t = Utils::Toml::GetOrCreateTablePath(doc.Root, p);
                    SetTomlValue(t, "ClassName", Utils::Toml::Value{.Storage = grandChild->GetClassName()});
                    writeProps(grandChild, t);
                    writeChildren(grandChild, p);
                }
            };
            writeChildren(child, childPath);
        }
    }

    out = Utils::Toml::Serialize(doc);
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
        if (it == props.end())
            continue;

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
