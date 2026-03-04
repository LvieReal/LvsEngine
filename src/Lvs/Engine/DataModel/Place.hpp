#pragma once

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/DataModel/DataModel.hpp"
#include "Lvs/Engine/DataModel/Service.hpp"

#include <QHash>
#include <QString>

#include <memory>

class QXmlStreamReader;
class QXmlStreamWriter;

namespace Lvs::Engine::DataModel {

class Place {
public:
    Place();

    static std::shared_ptr<Place> LoadFromFile(const QString& filePath);

    void SaveToFile(const QString& filePath);

    std::shared_ptr<DataModel> GetDataModel() const;
    std::shared_ptr<Core::Instance> FindInstanceById(const QString& instanceId) const;

    std::shared_ptr<Service> FindService(const QString& name) const;

    QString GetFilePath() const;
    void SetFilePath(const QString& path);

    bool IsDirty() const;
    void MarkDirty(bool value = true);

    void Destroy();

private:
    void CreateDefaultScene();
    void LoadFromXmlRoot(QXmlStreamReader& reader);
    void PrepareServiceForLoad(const std::shared_ptr<Service>& service);
    void SerializeInstanceRecursive(const std::shared_ptr<Core::Instance>& instance, QXmlStreamWriter& writer) const;
    std::shared_ptr<Core::Instance> DeserializeInstanceRecursive(QXmlStreamReader& reader, const std::shared_ptr<Core::Instance>& parent);
    void SerializeProperties(const std::shared_ptr<Core::Instance>& instance, QXmlStreamWriter& writer) const;
    void DeserializeProperties(const std::shared_ptr<Core::Instance>& instance, QXmlStreamReader& reader);
    QString EncodeValue(const QVariant& value) const;
    QVariant DecodeValue(const Core::PropertyDefinition& definition, const QString& rawValue) const;

    QString filePath_;
    bool dirty_{false};
    std::shared_ptr<DataModel> dataModel_;
    QHash<QString, std::shared_ptr<Service>> services_;
};

} // namespace Lvs::Engine::DataModel
