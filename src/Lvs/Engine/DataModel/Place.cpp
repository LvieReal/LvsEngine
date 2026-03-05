#include "Lvs/Engine/DataModel/Place.hpp"

#include "Lvs/Engine/DataModel/ChangeHistoryService.hpp"
#include "Lvs/Engine/DataModel/ClassRegistry.hpp"
#include "Lvs/Engine/DataModel/Lighting.hpp"
#include "Lvs/Engine/DataModel/Selection.hpp"
#include "Lvs/Engine/DataModel/ServiceRegistry.hpp"
#include "Lvs/Engine/DataModel/Workspace.hpp"
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
#include "Lvs/Engine/Objects/MeshPart.hpp"
#include "Lvs/Engine/Objects/Part.hpp"
#include "Lvs/Engine/Objects/SelectionBox.hpp"
#include "Lvs/Engine/Objects/Skybox.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QVariant>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <array>
#include <stdexcept>

namespace Lvs::Engine::DataModel {

namespace {
constexpr auto FILE_FORMAT_VERSION = "1";

void EnsureBuiltinRegistrations() {
    static const bool initialized = []() {
        (void)DataModel::Descriptor();
        (void)Service::Descriptor();
        (void)Workspace::Descriptor();
        (void)Lighting::Descriptor();
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
        return true;
    }();
    static_cast<void>(initialized);
}

void EnsureWorkspaceCurrentCamera(const std::shared_ptr<Workspace>& workspace) {
    if (workspace == nullptr) {
        return;
    }

    auto currentCamera = workspace->GetProperty("CurrentCamera").value<std::shared_ptr<Objects::Camera>>();
    if (currentCamera != nullptr && currentCamera->GetParent() != nullptr) {
        return;
    }

    for (const auto& child : workspace->GetChildren()) {
        if (const auto camera = std::dynamic_pointer_cast<Objects::Camera>(child); camera != nullptr) {
            workspace->SetProperty("CurrentCamera", QVariant::fromValue(camera));
            return;
        }
    }

    auto camera = std::make_shared<Objects::Camera>();
    camera->SetParent(workspace);
    workspace->SetProperty("CurrentCamera", QVariant::fromValue(camera));
}

} // namespace

Place::Place() {
    CreateDefaultScene();
}

std::shared_ptr<Place> Place::LoadFromFile(const QString& filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        throw std::runtime_error(QString("Failed to open place file: %1").arg(filePath).toStdString());
    }

    QXmlStreamReader reader(&file);
    if (!reader.readNextStartElement() || reader.name() != "Place") {
        throw std::runtime_error("Invalid place file: expected root <Place>.");
    }

    auto place = std::make_shared<Place>();
    place->LoadFromXmlRoot(reader);
    place->SetFilePath(filePath);
    place->MarkDirty(false);
    return place;
}

void Place::SaveToFile(const QString& filePath) {
    QFileInfo info(filePath);
    QDir().mkpath(info.absolutePath());

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
        throw std::runtime_error(QString("Failed to write place file: %1").arg(filePath).toStdString());
    }

    QXmlStreamWriter writer(&file);
    writer.setAutoFormatting(true);
    writer.writeStartDocument();
    writer.writeStartElement("Place");
    writer.writeAttribute("Version", FILE_FORMAT_VERSION);

    for (auto it = services_.cbegin(); it != services_.cend(); ++it) {
        const auto& service = it.value();
        if (service->IsHiddenService()) {
            continue;
        }

        writer.writeStartElement("Service");
        writer.writeAttribute("Name", it.key());
        SerializeProperties(service, writer);

        writer.writeStartElement("Instances");
        for (const auto& child : service->GetChildren()) {
            SerializeInstanceRecursive(child, writer);
        }
        writer.writeEndElement(); // Instances
        writer.writeEndElement(); // Service
    }

    writer.writeEndElement(); // Place
    writer.writeEndDocument();

    SetFilePath(filePath);
    MarkDirty(false);
}

std::shared_ptr<DataModel> Place::GetDataModel() const {
    return dataModel_;
}

std::shared_ptr<Core::Instance> Place::FindInstanceById(const QString& instanceId) const {
    return dataModel_->FindInstanceById(instanceId);
}

std::shared_ptr<Service> Place::FindService(const QString& name) const {
    return services_.value(name);
}

QString Place::GetFilePath() const {
    return filePath_;
}

void Place::SetFilePath(const QString& path) {
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

    for (auto& service : services_) {
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
        services_.insert(service->GetClassName(), service);

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
        basePlate->SetProperty("Size", QVariant::fromValue(Math::Vector3{128.0, 4.0, 128.0}));
        basePlate->SetParent(workspaceService);
    }

    MarkDirty(false);
}

void Place::LoadFromXmlRoot(QXmlStreamReader& reader) {
    while (reader.readNextStartElement()) {
        if (reader.name() != "Service") {
            reader.skipCurrentElement();
            continue;
        }

        const QString serviceName = reader.attributes().value("Name").toString();
        auto service = FindService(serviceName);
        if (service == nullptr) {
            reader.skipCurrentElement();
            continue;
        }

        PrepareServiceForLoad(service);

        while (reader.readNextStartElement()) {
            if (reader.name() == "Properties") {
                DeserializeProperties(service, reader);
                continue;
            }
            if (reader.name() == "Instances") {
                while (reader.readNextStartElement()) {
                    if (reader.name() == "Instance") {
                        DeserializeInstanceRecursive(reader, service);
                    } else {
                        reader.skipCurrentElement();
                    }
                }
                continue;
            }
            reader.skipCurrentElement();
        }
    }

    EnsureWorkspaceCurrentCamera(std::dynamic_pointer_cast<Workspace>(FindService("Workspace")));
}

void Place::PrepareServiceForLoad(const std::shared_ptr<Service>& service) {
    for (const auto& child : service->GetChildren()) {
        if (child->IsInsertable()) {
            child->Destroy();
        }
    }
}

void Place::SerializeInstanceRecursive(const std::shared_ptr<Core::Instance>& instance, QXmlStreamWriter& writer) const {
    if (!instance->IsInsertable()) {
        return;
    }

    writer.writeStartElement("Instance");
    writer.writeAttribute("Class", instance->GetClassName());
    SerializeProperties(instance, writer);

    writer.writeStartElement("Children");
    for (const auto& child : instance->GetChildren()) {
        SerializeInstanceRecursive(child, writer);
    }
    writer.writeEndElement(); // Children
    writer.writeEndElement(); // Instance
}

std::shared_ptr<Core::Instance> Place::DeserializeInstanceRecursive(
    QXmlStreamReader& reader,
    const std::shared_ptr<Core::Instance>& parent
) {
    const QString className = reader.attributes().value("Class").toString();
    if (className.isEmpty()) {
        reader.skipCurrentElement();
        return nullptr;
    }

    auto instance = ClassRegistry::CreateInstance(className);

    while (reader.readNextStartElement()) {
        if (reader.name() == "Properties") {
            DeserializeProperties(instance, reader);
            continue;
        }
        if (reader.name() == "Children") {
            while (reader.readNextStartElement()) {
                if (reader.name() == "Instance") {
                    DeserializeInstanceRecursive(reader, instance);
                } else {
                    reader.skipCurrentElement();
                }
            }
            continue;
        }
        reader.skipCurrentElement();
    }

    instance->SetParent(parent);
    return instance;
}

void Place::SerializeProperties(const std::shared_ptr<Core::Instance>& instance, QXmlStreamWriter& writer) const {
    writer.writeStartElement("Properties");

    for (auto it = instance->GetProperties().cbegin(); it != instance->GetProperties().cend(); ++it) {
        const auto& property = it.value();
        const auto& definition = property.Definition();

        if (!definition.Serializable || definition.Name == "ClassName" || definition.IsInstanceReference) {
            continue;
        }

        const QString encoded = EncodeValue(property.Get());
        writer.writeStartElement("Property");
        writer.writeAttribute("Name", definition.Name);
        writer.writeAttribute("Type", QString::fromUtf8(definition.Type.name()));
        writer.writeCharacters(encoded);
        writer.writeEndElement();
    }

    writer.writeEndElement(); // Properties
}

void Place::DeserializeProperties(const std::shared_ptr<Core::Instance>& instance, QXmlStreamReader& reader) {
    while (reader.readNextStartElement()) {
        if (reader.name() != "Property") {
            reader.skipCurrentElement();
            continue;
        }

        const QString propertyName = reader.attributes().value("Name").toString();
        const QString rawValue = reader.readElementText();
        if (propertyName.isEmpty() || propertyName == "ClassName") {
            continue;
        }

        const auto& props = instance->GetProperties();
        const auto it = props.constFind(propertyName);
        if (it == props.cend()) {
            continue;
        }

        const auto& definition = it->Definition();
        if (definition.IsInstanceReference) {
            continue;
        }

        instance->SetProperty(propertyName, DecodeValue(definition, rawValue));
    }
}

QString Place::EncodeValue(const QVariant& value) const {
    if (!value.isValid() || value.isNull()) {
        return {};
    }

    if (value.typeId() == QMetaType::Bool) {
        return value.toBool() ? "true" : "false";
    }
    if (value.typeId() == QMetaType::Int) {
        return QString::number(value.toInt());
    }
    if (value.typeId() == QMetaType::Double) {
        return QString::number(value.toDouble(), 'g', 17);
    }
    if (value.typeId() == QMetaType::fromType<Math::Vector3>().id()) {
        const auto v = value.value<Math::Vector3>();
        return QString("%1,%2,%3")
            .arg(QString::number(v.x, 'g', 17))
            .arg(QString::number(v.y, 'g', 17))
            .arg(QString::number(v.z, 'g', 17));
    }
    if (value.typeId() == QMetaType::fromType<Math::Color3>().id()) {
        const auto c = value.value<Math::Color3>();
        return QString("%1,%2,%3")
            .arg(QString::number(c.r, 'g', 17))
            .arg(QString::number(c.g, 'g', 17))
            .arg(QString::number(c.b, 'g', 17));
    }
    if (value.typeId() == QMetaType::fromType<Math::CFrame>().id()) {
        const auto c = value.value<Math::CFrame>();
        const auto p = c.Position;
        const auto r = c.Right;
        const auto u = c.Up;
        const auto b = c.Back;
        return QString("%1,%2,%3;%4,%5,%6;%7,%8,%9;%10,%11,%12")
            .arg(QString::number(p.x, 'g', 17))
            .arg(QString::number(p.y, 'g', 17))
            .arg(QString::number(p.z, 'g', 17))
            .arg(QString::number(r.x, 'g', 17))
            .arg(QString::number(r.y, 'g', 17))
            .arg(QString::number(r.z, 'g', 17))
            .arg(QString::number(u.x, 'g', 17))
            .arg(QString::number(u.y, 'g', 17))
            .arg(QString::number(u.z, 'g', 17))
            .arg(QString::number(b.x, 'g', 17))
            .arg(QString::number(b.y, 'g', 17))
            .arg(QString::number(b.z, 'g', 17));
    }
    if (Enums::Metadata::IsRegisteredEnumType(value.typeId())) {
        return QString::number(Enums::Metadata::IntFromVariant(value));
    }

    return value.toString();
}

QVariant Place::DecodeValue(const Core::PropertyDefinition& definition, const QString& rawValue) const {
    const int typeId = definition.Type.id();

    if (typeId == QMetaType::QString) {
        return rawValue;
    }
    if (typeId == QMetaType::Bool) {
        const QString normalized = rawValue.trimmed().toLower();
        return normalized == "1" || normalized == "true" || normalized == "yes" || normalized == "on";
    }
    if (typeId == QMetaType::Int) {
        return rawValue.toInt();
    }
    if (typeId == QMetaType::Double) {
        return rawValue.toDouble();
    }
    if (typeId == QMetaType::fromType<Math::Vector3>().id()) {
        const auto parts = rawValue.split(',');
        if (parts.size() != 3) {
            throw std::runtime_error("Invalid Vector3 serialized value.");
        }
        return QVariant::fromValue(Math::Vector3{
            parts[0].toDouble(),
            parts[1].toDouble(),
            parts[2].toDouble()
        });
    }
    if (typeId == QMetaType::fromType<Math::Color3>().id()) {
        const auto parts = rawValue.split(',');
        if (parts.size() != 3) {
            throw std::runtime_error("Invalid Color3 serialized value.");
        }
        return QVariant::fromValue(Math::Color3{
            parts[0].toDouble(),
            parts[1].toDouble(),
            parts[2].toDouble()
        });
    }
    if (typeId == QMetaType::fromType<Math::CFrame>().id()) {
        const auto segs = rawValue.split(';');
        if (segs.size() != 4) {
            throw std::runtime_error("Invalid CFrame serialized value.");
        }
        auto parseVec = [](const QString& part) -> Math::Vector3 {
            const auto vals = part.split(',');
            if (vals.size() != 3) {
                throw std::runtime_error("Invalid CFrame vector segment.");
            }
            return {vals[0].toDouble(), vals[1].toDouble(), vals[2].toDouble()};
        };
        return QVariant::fromValue(Math::CFrame{
            parseVec(segs[0]),
            parseVec(segs[1]),
            parseVec(segs[2]),
            parseVec(segs[3])
        });
    }
    if (Enums::Metadata::IsRegisteredEnumType(typeId)) {
        return Enums::Metadata::VariantFromInt(typeId, rawValue.toInt());
    }

    QVariant value = rawValue;
    value.convert(definition.Type);
    return value;
}

} // namespace Lvs::Engine::DataModel
