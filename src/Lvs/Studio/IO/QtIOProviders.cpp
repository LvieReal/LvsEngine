#include "Lvs/Studio/IO/QtIOProviders.hpp"

#include "Lvs/Engine/IO/Providers.hpp"

#include "Lvs/Qt/QtBridge.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QXmlStreamReader>
#include <QXmlStreamWriter>

#include <cstring>
#include <memory>

namespace Lvs::Studio::IO {

namespace {

class QtFileSystem final : public Engine::IO::IFileSystem {
public:
    [[nodiscard]] bool Exists(const Engine::Core::String& path) const override {
        return QFileInfo::exists(Engine::Core::QtBridge::ToQString(path));
    }

    [[nodiscard]] std::optional<Engine::Core::String> ReadAllText(const Engine::Core::String& path) const override {
        QFile file(Engine::Core::QtBridge::ToQString(path));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return std::nullopt;
        }
        return Engine::Core::QtBridge::ToStdString(QString::fromUtf8(file.readAll()));
    }

    [[nodiscard]] std::optional<ByteBuffer> ReadAllBytes(const Engine::Core::String& path) const override {
        QFile file(Engine::Core::QtBridge::ToQString(path));
        if (!file.open(QIODevice::ReadOnly)) {
            return std::nullopt;
        }
        const QByteArray bytes = file.readAll();
        ByteBuffer out;
        out.resize(static_cast<size_t>(bytes.size()));
        if (!out.empty()) {
            std::memcpy(out.data(), bytes.constData(), out.size());
        }
        return out;
    }

    [[nodiscard]] bool WriteAllText(const Engine::Core::String& path, const Engine::Core::String& data) const override {
        QFileInfo info(Engine::Core::QtBridge::ToQString(path));
        QDir().mkpath(info.absolutePath());

        QFile file(info.filePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text | QIODevice::Truncate)) {
            return false;
        }
        const QByteArray bytes = QString::fromUtf8(data.c_str()).toUtf8();
        return file.write(bytes) == bytes.size();
    }

    [[nodiscard]] bool WriteAllBytes(const Engine::Core::String& path, const ByteBuffer& data) const override {
        QFileInfo info(Engine::Core::QtBridge::ToQString(path));
        QDir().mkpath(info.absolutePath());

        QFile file(info.filePath());
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            return false;
        }
        const QByteArray bytes(reinterpret_cast<const char*>(data.data()), static_cast<qsizetype>(data.size()));
        return file.write(bytes) == bytes.size();
    }

    [[nodiscard]] bool MkdirP(const Engine::Core::String& path) const override {
        if (path.empty()) {
            return true;
        }
        return QDir().mkpath(Engine::Core::QtBridge::ToQString(path));
    }
};

class QtXmlReader final : public Engine::IO::IXmlReader {
public:
    explicit QtXmlReader(const Engine::Core::String& text) : source_(Engine::Core::QtBridge::ToQString(text)), reader_(source_) {}

    [[nodiscard]] bool ReadNextStartElement() override { return reader_.readNextStartElement(); }
    [[nodiscard]] Engine::Core::String Name() const override { return Engine::Core::QtBridge::ToStdString(reader_.name().toString()); }

    [[nodiscard]] Engine::Core::HashMap<Engine::Core::String, Engine::Core::String> Attributes() const override {
        Engine::Core::HashMap<Engine::Core::String, Engine::Core::String> attrs;
        const auto qattrs = reader_.attributes();
        attrs.reserve(static_cast<size_t>(qattrs.size()));
        for (const auto& a : qattrs) {
            attrs.insert_or_assign(
                Engine::Core::QtBridge::ToStdString(a.name().toString()),
                Engine::Core::QtBridge::ToStdString(a.value().toString())
            );
        }
        return attrs;
    }

    [[nodiscard]] Engine::Core::String ReadElementText() override {
        return Engine::Core::QtBridge::ToStdString(reader_.readElementText());
    }

    void SkipCurrentElement() override { reader_.skipCurrentElement(); }

    [[nodiscard]] bool HasError() const override { return reader_.hasError(); }
    [[nodiscard]] Engine::Core::String ErrorString() const override {
        return Engine::Core::QtBridge::ToStdString(reader_.errorString());
    }

private:
    QString source_;
    QXmlStreamReader reader_;
};

class QtXmlWriter final : public Engine::IO::IXmlWriter {
public:
    QtXmlWriter() : writer_(&text_) { writer_.setAutoFormatting(true); }

    void WriteStartDocument() override { writer_.writeStartDocument(); }
    void WriteEndDocument() override { writer_.writeEndDocument(); }

    void WriteStartElement(const Engine::Core::String& name) override {
        writer_.writeStartElement(Engine::Core::QtBridge::ToQString(name));
    }
    void WriteEndElement() override { writer_.writeEndElement(); }

    void WriteAttribute(const Engine::Core::String& name, const Engine::Core::String& value) override {
        writer_.writeAttribute(Engine::Core::QtBridge::ToQString(name), Engine::Core::QtBridge::ToQString(value));
    }

    void WriteCharacters(const Engine::Core::String& text) override { writer_.writeCharacters(Engine::Core::QtBridge::ToQString(text)); }

    [[nodiscard]] Engine::Core::String GetText() const override { return Engine::Core::QtBridge::ToStdString(text_); }

private:
    QString text_;
    QXmlStreamWriter writer_;
};

class QtXml final : public Engine::IO::IXml {
public:
    [[nodiscard]] std::unique_ptr<Engine::IO::IXmlReader> CreateReaderFromText(const Engine::Core::String& text) const override {
        return std::make_unique<QtXmlReader>(text);
    }

    [[nodiscard]] std::unique_ptr<Engine::IO::IXmlWriter> CreateWriter() const override { return std::make_unique<QtXmlWriter>(); }
};

} // namespace

void RegisterQtIOProviders() {
    Engine::IO::Providers::SetFileSystem(std::make_shared<QtFileSystem>());
    Engine::IO::Providers::SetXml(std::make_shared<QtXml>());
}

} // namespace Lvs::Studio::IO
