#include "Lvs/Engine/Core/ExternalMetadata.hpp"

#include "Lvs/Engine/Core/ClassDescriptor.hpp"
#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Core/PropertyTags.hpp"
#include "Lvs/Engine/Enums/EnumMetadata.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Utils/Json.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <optional>
#include <string>
#include <utility>

namespace Lvs::Engine::Core {

namespace {

[[nodiscard]] std::optional<Core::String> ReadAllText(const Core::String& path) {
    if (path.empty()) {
        return std::nullopt;
    }

    std::ifstream file(path, std::ios::in | std::ios::binary);
    if (!file.is_open()) {
        return std::nullopt;
    }

    std::string contents;
    file.seekg(0, std::ios::end);
    const std::streamoff size = file.tellg();
    if (size < 0) {
        return std::nullopt;
    }
    contents.resize(static_cast<std::size_t>(size));
    file.seekg(0, std::ios::beg);
    file.read(contents.data(), static_cast<std::streamsize>(contents.size()));
    if (!file) {
        return std::nullopt;
    }
    return Core::String(contents.data(), contents.size());
}

 [[nodiscard]] TypeId ParseTypeId(const StringView typeName, bool& isInstanceRefOut) {
    auto toLower = [](StringView s) {
        String out;
        out.reserve(s.size());
        for (const char ch : s) {
            out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        return out;
    };

    const String lowered = toLower(typeName);
    isInstanceRefOut = false;

    if (lowered == "bool" || lowered == "boolean") {
        return TypeId::Bool;
    }
    if (lowered == "int" || lowered == "int64" || lowered == "integer") {
        return TypeId::Int;
    }
    if (lowered == "double" || lowered == "float" || lowered == "number") {
        return TypeId::Double;
    }
    if (lowered == "string") {
        return TypeId::String;
    }
    if (lowered == "vector3") {
        return TypeId::Vector3;
    }
    if (lowered == "color3") {
        return TypeId::Color3;
    }
    if (lowered == "cframe") {
        return TypeId::CFrame;
    }
    if (lowered == "enum") {
        return TypeId::Enum;
    }
    if (lowered == "instanceref" || lowered == "instance_ref" || lowered == "instance") {
        isInstanceRefOut = true;
        return TypeId::InstanceRef;
    }
    if (lowered == "bytearray" || lowered == "bytes") {
        return TypeId::ByteArray;
    }

     return TypeId::Invalid;
 }
 
 [[nodiscard]] std::optional<Math::Vector3> ParseVector3(const Utils::Json::Value& value) {
     if (!value.IsArray()) {
         return std::nullopt;
     }
     const auto& arr = value.AsArray();
     if (arr.size() != 3) {
         return std::nullopt;
     }
     return Math::Vector3{arr[0].AsNumber(), arr[1].AsNumber(), arr[2].AsNumber()};
 }
 
 [[nodiscard]] std::optional<Math::Color3> ParseColor3(const Utils::Json::Value& value) {
     if (!value.IsArray()) {
         return std::nullopt;
     }
     const auto& arr = value.AsArray();
     if (arr.size() != 3) {
         return std::nullopt;
     }
     return Math::Color3{arr[0].AsNumber(), arr[1].AsNumber(), arr[2].AsNumber()};
 }
 
 [[nodiscard]] std::optional<Math::CFrame> ParseCFrame(const Utils::Json::Value& value) {
     if (value.IsString()) {
         String lowered;
         lowered.reserve(value.AsString().size());
         for (const char ch : value.AsString()) {
            lowered.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(ch))));
        }
        if (lowered == "identity") {
            return Math::CFrame::Identity();
        }
        return std::nullopt;
    }
 
     if (!value.IsArray()) {
         return std::nullopt;
     }
     const auto& arr = value.AsArray();
     if (arr.size() == 12) {
         const Math::Vector3 p{arr[0].AsNumber(), arr[1].AsNumber(), arr[2].AsNumber()};
         const Math::Vector3 r{arr[3].AsNumber(), arr[4].AsNumber(), arr[5].AsNumber()};
         const Math::Vector3 u{arr[6].AsNumber(), arr[7].AsNumber(), arr[8].AsNumber()};
         const Math::Vector3 b{arr[9].AsNumber(), arr[10].AsNumber(), arr[11].AsNumber()};
         return Math::CFrame{p, r, u, b};
     }
     if (arr.size() == 4) {
         const auto p = ParseVector3(arr[0]);
        const auto r = ParseVector3(arr[1]);
        const auto u = ParseVector3(arr[2]);
        const auto b = ParseVector3(arr[3]);
        if (p && r && u && b) {
            return Math::CFrame{*p, *r, *u, *b};
        }
    }

     return std::nullopt;
 }
 
 [[nodiscard]] Variant JsonToVariant(const Utils::Json::Value& v) {
     if (v.IsBool()) {
         return Variant::From(v.AsBool());
     }
     if (v.IsNumber()) {
         const double d = v.AsNumber();
         if (std::isfinite(d) && std::floor(d) == d) {
             return Variant::From(static_cast<int64_t>(d));
         }
         return Variant::From(d);
     }
     if (v.IsString()) {
         return Variant::From(v.AsString());
     }
     return Variant{};
 }
 
 [[nodiscard]] Variant ParseDefaultValue(
     const TypeId type,
     const bool isInstanceRef,
     const String& enumType,
     const Utils::Json::Value& value
 ) {
     if (isInstanceRef) {
         return Variant::From(Variant::InstanceRef{});
     }
 
     switch (type) {
     case TypeId::Bool:
         return Variant::From(value.AsBool());
     case TypeId::Int:
         return Variant::From(static_cast<int64_t>(value.AsNumber()));
     case TypeId::Double:
         return Variant::From(value.AsNumber());
     case TypeId::String:
         return Variant::From(value.AsString());
     case TypeId::Vector3: {
         const auto v3 = ParseVector3(value);
         return v3 ? Variant::From(*v3) : Variant::From(Math::Vector3{});
     }
     case TypeId::Color3: {
         const auto c3 = ParseColor3(value);
         return c3 ? Variant::From(*c3) : Variant::From(Math::Color3{});
     }
     case TypeId::CFrame: {
         const auto cf = ParseCFrame(value);
         return cf ? Variant::From(*cf) : Variant::From(Math::CFrame::Identity());
     }
     case TypeId::Enum: {
         if (value.IsString()) {
             if (!enumType.empty() && Enums::Metadata::IsRegisteredEnum(enumType)) {
                 return Enums::Metadata::VariantFromName(enumType, value.AsString());
             }
             // Enum names require enumType to be set; fall back to 0.
             return Variant::From(0);
         }
         return Variant::From(static_cast<int>(value.AsNumber()));
     }
     case TypeId::InstanceRef:
         return Variant::From(Variant::InstanceRef{});
     case TypeId::ByteArray:
    case TypeId::Invalid:
        return Variant{};
     }
     return Variant{};
 }
 
 [[nodiscard]] StringList ParseTags(const Utils::Json::Object& propObject) {
     StringList out;
 
     if (const auto* tagsV = Utils::Json::Find(propObject, "tags"); tagsV && tagsV->IsArray()) {
         for (const auto& t : tagsV->AsArray()) {
             if (t.IsString()) {
                 out.push_back(t.AsString());
             }
         }
     }
 
     return out;
 }
 
 [[nodiscard]] HashMap<String, Variant> ParseAttributes(const Utils::Json::Object& propObject) {
     HashMap<String, Variant> out;
     const auto* attrsV = Utils::Json::Find(propObject, "attributes");
     if (attrsV == nullptr || !attrsV->IsObject()) {
         return out;
     }
 
     for (const auto& [k, v] : attrsV->AsObject()) {
         out.insert_or_assign(k, JsonToVariant(v));
     }
     return out;
 }
 
 [[nodiscard]] int ParseIntOrDefault(const Utils::Json::Value* value, const int fallback) {
     if (value == nullptr) {
         return fallback;
     }
     if (value->IsNumber()) {
         return static_cast<int>(value->AsNumber(fallback));
     }
     return fallback;
 }
 
 } // namespace

ExternalMetadata& ExternalMetadata::Get() {
    static ExternalMetadata instance;
    return instance;
}

void ExternalMetadata::EnsureLoaded() {
    std::lock_guard lock(mutex_);
    if (loaded_) {
        return;
    }
    metadataPath_ = ResolveMetadataPath();
    lastModified_ = QueryLastModified(metadataPath_);
    loaded_ = LoadFromDiskLocked();
    if (loaded_) {
        ApplyLocked();
        SyncRegisteredRootsLocked();
    }
}

void ExternalMetadata::PollHotReload() {
    std::unique_lock lock(mutex_);
    if (!loaded_) {
        lock.unlock();
        EnsureLoaded();
        return;
    }

    if (metadataPath_.empty()) {
        return;
    }
    const auto modified = QueryLastModified(metadataPath_);
    if (!modified.has_value()) {
        return;
    }
    if (lastModified_.has_value() && *modified <= *lastModified_) {
        return;
    }

    lastModified_ = modified;
    if (!LoadFromDiskLocked()) {
        return;
    }
    ApplyLocked();
    SyncRegisteredRootsLocked();
    lock.unlock();
    Reloaded.Fire();
}

void ExternalMetadata::RegisterRoot(const std::shared_ptr<Instance>& root) {
    if (root == nullptr) {
        return;
    }
    std::lock_guard lock(mutex_);
    const auto already = std::find_if(roots_.begin(), roots_.end(), [&](const std::weak_ptr<Instance>& existing) {
        const auto locked = existing.lock();
        return locked && locked.get() == root.get();
    });
    if (already == roots_.end()) {
        roots_.push_back(root);
    }
}

void ExternalMetadata::UnregisterRoot(const std::shared_ptr<Instance>& root) {
    if (root == nullptr) {
        return;
    }
    std::lock_guard lock(mutex_);
    roots_.erase(
        std::remove_if(
            roots_.begin(),
            roots_.end(),
            [&](const std::weak_ptr<Instance>& existing) {
                const auto locked = existing.lock();
                return !locked || locked.get() == root.get();
            }
        ),
        roots_.end()
    );
}

String ExternalMetadata::GetMetadataPath() const {
    std::lock_guard lock(mutex_);
    return metadataPath_;
}

String ExternalMetadata::GetLastLoadedMetadataText() const {
    std::lock_guard lock(mutex_);
    return lastLoadedMetadataText_;
}

String ExternalMetadata::ResolveMetadataPath() const {
    const String json = Utils::SourcePath::GetSourcePath("config/reflection/Objects.json");
    if (QueryLastModified(json).has_value()) {
        return json;
    }
    return {};
}

std::optional<std::filesystem::file_time_type> ExternalMetadata::QueryLastModified(const String& osPath) const {
    if (osPath.empty()) {
        return std::nullopt;
    }
    std::error_code ec;
    const auto p = std::filesystem::path(osPath);
    if (!std::filesystem::exists(p, ec)) {
        return std::nullopt;
    }
    const auto time = std::filesystem::last_write_time(p, ec);
    if (ec) {
        return std::nullopt;
    }
    return time;
}

bool ExternalMetadata::LoadFromDiskLocked() {
    const auto text = ReadAllText(metadataPath_);
    if (!text) {
        return false;
    }

    metadataText_ = *text;
    lastLoadedMetadataText_ = *text;
    return true;
}

void ExternalMetadata::ApplyLocked() {
    if (metadataText_.empty()) {
        return;
    }

    Utils::Json::Value doc;
    try {
        doc = Utils::Json::Parse(metadataText_);
    } catch (...) {
        return;
    }
    if (!doc.IsObject()) {
        return;
    }
    const auto* classesV = Utils::Json::Find(doc.AsObject(), "classes");
    if (classesV == nullptr || !classesV->IsObject()) {
        return;
    }

    struct PendingClass {
        String Name;
        const ClassDescriptor* Descriptor{nullptr};
        const Utils::Json::Object* Object{nullptr};
        int Depth{0};
    };

    std::vector<PendingClass> pending;
    pending.reserve(classesV->AsObject().size());

    for (const auto& [className, classV] : classesV->AsObject()) {
        const auto* desc = ClassDescriptor::Get(className);
        if (desc == nullptr) {
            continue;
        }
        if (!classV.IsObject()) {
            continue;
        }
        PendingClass entry;
        entry.Name = className;
        entry.Descriptor = desc;
        entry.Object = &classV.AsObject();
        pending.push_back(entry);
    }

    for (auto& p : pending) {
        int depth = 0;
        const ClassDescriptor* current = p.Descriptor != nullptr ? p.Descriptor->BaseDescriptor() : nullptr;
        while (current != nullptr) {
            ++depth;
            current = current->BaseDescriptor();
        }
        p.Depth = depth;
    }
    std::sort(pending.begin(), pending.end(), [](const PendingClass& a, const PendingClass& b) {
        if (a.Depth != b.Depth) {
            return a.Depth < b.Depth;
        }
        return a.Name < b.Name;
    });

    for (const auto& cls : pending) {
        auto* descriptor = const_cast<ClassDescriptor*>(cls.Descriptor);
        if (descriptor == nullptr || cls.Object == nullptr) {
            continue;
        }

        descriptor->ResetPropertiesToBase();

        const auto* categoriesV = Utils::Json::Find(*cls.Object, "categories");
        if (categoriesV == nullptr || !categoriesV->IsObject()) {
            continue;
        }

        for (const auto& [categoryName, categoryV] : categoriesV->AsObject()) {
            if (!categoryV.IsObject()) {
                continue;
            }
            for (const auto& [propName, propV] : categoryV.AsObject()) {
                if (!propV.IsObject()) {
                    continue;
                }
                const auto& propObj = propV.AsObject();
                const auto* typeV = Utils::Json::Find(propObj, "type");
                if (typeV == nullptr || !typeV->IsString()) {
                    continue;
                }
                bool isInstanceRef = false;
                const TypeId typeId = ParseTypeId(typeV->AsString(), isInstanceRef);

                HashMap<String, Variant> attributes = ParseAttributes(propObj);

                String enumType;
                if (typeId == TypeId::Enum) {
                    const auto* enumTypeV = Utils::Json::Find(propObj, "enumType");
                    if (enumTypeV && enumTypeV->IsString()) {
                        enumType = enumTypeV->AsString();
                        attributes.insert_or_assign("EnumType", Variant::From(enumType));
                    }
                }

                const auto* serializableV = Utils::Json::Find(propObj, "serializable");
                const bool serializable = (serializableV && serializableV->IsBool()) ? serializableV->AsBool(true) : true;
                const auto* descV = Utils::Json::Find(propObj, "description");
                const String description = (descV && descV->IsString()) ? descV->AsString() : String{};
                const auto* readOnlyV = Utils::Json::Find(propObj, "readOnly");
                const bool readOnly = (readOnlyV && readOnlyV->IsBool()) ? readOnlyV->AsBool(false) : false;

                Variant defaultValue = Variant{};
                if (const auto* defV = Utils::Json::Find(propObj, "default"); defV != nullptr) {
                    defaultValue = ParseDefaultValue(typeId, isInstanceRef, enumType, *defV);
                } else {
                    if (isInstanceRef) {
                        defaultValue = Variant::From(Variant::InstanceRef{});
                    } else if (typeId == TypeId::CFrame) {
                        defaultValue = Variant::From(Math::CFrame::Identity());
                    } else if (typeId == TypeId::Color3) {
                        defaultValue = Variant::From(Math::Color3{});
                    } else if (typeId == TypeId::Vector3) {
                        defaultValue = Variant::From(Math::Vector3{});
                    }
                }

                PropertyDefinition def;
                def.Name = propName;
                def.Type = typeId;
                def.Default = std::move(defaultValue);
                def.Serializable = serializable;
                def.Category = categoryName;
                def.Description = description;
                def.ReadOnly = readOnly;
                def.CustomTags = ParseTags(propObj);
                def.CustomAttributes = std::move(attributes);
                def.IsInstanceReference = isInstanceRef;
                def.RegistrationOrder = ParseIntOrDefault(Utils::Json::Find(propObj, "order"), -1);

                descriptor->RegisterProperty(def);
            }
        }
    }
}

void ExternalMetadata::SyncRegisteredRootsLocked() {
    roots_.erase(
        std::remove_if(roots_.begin(), roots_.end(), [](const std::weak_ptr<Instance>& w) { return w.expired(); }),
        roots_.end()
    );

    for (const auto& w : roots_) {
        const auto root = w.lock();
        if (!root) {
            continue;
        }
        root->SyncPropertiesFromDescriptor(true);
        root->ForEachDescendant([](const std::shared_ptr<Instance>& instance) {
            if (instance != nullptr) {
                instance->SyncPropertiesFromDescriptor(true);
            }
        });
    }
}

} // namespace Lvs::Engine::Core
