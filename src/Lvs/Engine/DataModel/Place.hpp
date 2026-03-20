#pragma once

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Core/Types.hpp"
#include "Lvs/Engine/DataModel/DataModel.hpp"
#include "Lvs/Engine/DataModel/Services/Service.hpp"
#include "Lvs/Engine/IO/IXml.hpp"

#include <memory>

namespace Lvs::Engine::DataModel {

class Place {
public:
    Place();

    static std::shared_ptr<Place> LoadFromFile(const Core::String& filePath);

    void SaveToFile(const Core::String& filePath);

    std::shared_ptr<DataModel> GetDataModel() const;
    std::shared_ptr<Core::Instance> FindInstanceById(const Core::String& instanceId) const;

    std::shared_ptr<Service> FindService(const Core::String& name) const;

    Core::String GetFilePath() const;
    void SetFilePath(const Core::String& path);

    bool IsDirty() const;
    void MarkDirty(bool value = true);

    void Destroy();

private:
    void CreateDefaultScene();
    void LoadFromXmlRoot(IO::IXmlReader& reader);
    void PrepareServiceForLoad(const std::shared_ptr<Service>& service);
    void SerializeInstanceRecursive(const std::shared_ptr<Core::Instance>& instance, IO::IXmlWriter& writer) const;
    std::shared_ptr<Core::Instance> DeserializeInstanceRecursive(IO::IXmlReader& reader, const std::shared_ptr<Core::Instance>& parent);
    void SerializeProperties(const std::shared_ptr<Core::Instance>& instance, IO::IXmlWriter& writer) const;
    void DeserializeProperties(const std::shared_ptr<Core::Instance>& instance, IO::IXmlReader& reader);
    Core::String EncodeValue(const Core::Variant& value) const;
    Core::Variant DecodeValue(const Core::PropertyDefinition& definition, const Core::String& rawValue) const;

    Core::String filePath_;
    bool dirty_{false};
    std::shared_ptr<DataModel> dataModel_;
    Core::HashMap<Core::String, std::shared_ptr<Service>> services_;
};

} // namespace Lvs::Engine::DataModel
