#pragma once

#include "Lvs/Engine/Core/Types.hpp"

#include <memory>

namespace Lvs::Engine::IO {

class IXmlReader {
public:
    virtual ~IXmlReader() = default;

    [[nodiscard]] virtual bool ReadNextStartElement() = 0;
    [[nodiscard]] virtual Core::String Name() const = 0;
    [[nodiscard]] virtual Core::HashMap<Core::String, Core::String> Attributes() const = 0;
    [[nodiscard]] virtual Core::String ReadElementText() = 0;
    virtual void SkipCurrentElement() = 0;

    [[nodiscard]] virtual bool HasError() const = 0;
    [[nodiscard]] virtual Core::String ErrorString() const = 0;
};

class IXmlWriter {
public:
    virtual ~IXmlWriter() = default;

    virtual void WriteStartDocument() = 0;
    virtual void WriteEndDocument() = 0;

    virtual void WriteStartElement(const Core::String& name) = 0;
    virtual void WriteEndElement() = 0;

    virtual void WriteAttribute(const Core::String& name, const Core::String& value) = 0;
    virtual void WriteCharacters(const Core::String& text) = 0;

    [[nodiscard]] virtual Core::String GetText() const = 0;
};

class IXml {
public:
    virtual ~IXml() = default;

    [[nodiscard]] virtual std::unique_ptr<IXmlReader> CreateReaderFromText(const Core::String& text) const = 0;
    [[nodiscard]] virtual std::unique_ptr<IXmlWriter> CreateWriter() const = 0;
};

} // namespace Lvs::Engine::IO

