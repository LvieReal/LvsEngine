#pragma once

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Core/Types.hpp"
#include "Lvs/Engine/DataModel/DataModel.hpp"
#include "Lvs/Engine/DataModel/Services/Service.hpp"
#include "Lvs/Engine/IO/IXml.hpp"

#include <memory>
#include <string_view>

namespace Lvs::Engine::Utils::Toml {
struct Table;
}

namespace Lvs::Engine::DataModel {

class Place {
public:
    enum class FileFormat {
        Binary,
        Xml,
        Toml
    };

    Place();

    static std::shared_ptr<Place> LoadFromFile(const Core::String& filePath);

    void SaveToFile(const Core::String& filePath);
    void SaveToFileAs(const Core::String& filePath, FileFormat format);

    [[nodiscard]] FileFormat GetLoadedFileFormat() const;
    [[nodiscard]] FileFormat GetPreferredSaveFormat() const;
    void SetPreferredSaveFormat(FileFormat format);

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
    void LoadFromTomlText(std::string_view text);
    void PrepareServiceForLoad(const std::shared_ptr<Service>& service);
    void SerializeInstanceRecursive(const std::shared_ptr<Core::Instance>& instance, IO::IXmlWriter& writer) const;
    std::shared_ptr<Core::Instance> DeserializeInstanceRecursive(IO::IXmlReader& reader, const std::shared_ptr<Core::Instance>& parent);
    void SerializeProperties(const std::shared_ptr<Core::Instance>& instance, IO::IXmlWriter& writer) const;
    void DeserializeProperties(const std::shared_ptr<Core::Instance>& instance, IO::IXmlReader& reader);
    void SerializeToToml(Core::String& out) const;
    void DeserializeInstanceRecursiveToml(
        const ::Lvs::Engine::Utils::Toml::Table& table,
        const Core::String& tableKeyName,
        const std::shared_ptr<Core::Instance>& parent
    );
    Core::String EncodeValue(const Core::Variant& value) const;
    Core::Variant DecodeValue(const Core::PropertyDefinition& definition, const Core::String& rawValue) const;

    Core::String filePath_;
    FileFormat loadedFormat_{FileFormat::Binary};
    FileFormat preferredSaveFormat_{FileFormat::Binary};
    bool dirty_{false};
    std::shared_ptr<DataModel> dataModel_;
    Core::HashMap<Core::String, std::shared_ptr<Service>> services_;
};

} // namespace Lvs::Engine::DataModel
