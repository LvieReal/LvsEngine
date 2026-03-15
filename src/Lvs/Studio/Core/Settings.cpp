#include "Lvs/Studio/Core/Settings.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

#include "Lvs/Engine/Enums/EnumMetadata.hpp"
#include "Lvs/Engine/Enums/MSAA.hpp"
#include "Lvs/Engine/Enums/SurfaceMipmapping.hpp"
#include "Lvs/Engine/Enums/Theme.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>
#include <stdexcept>
#include <vector>

namespace Lvs::Studio::Core::Settings {

namespace {

QMap<QString, SettingMeta> g_settings = {
    {"BaseCameraSpeed", {"Base Camera Speed", "Base camera speed", 15.0}},
    {"ShiftCameraSpeed", {"Shift Camera Speed", "Shift camera speed", 5.0}},
    {"Theme", {"Theme", "Main studio theme", QVariant::fromValue(Engine::Enums::Theme::Light)}},
    {"ExplorerIconPack", {"Explorer Icon Pack", "Explorer icon pack folder name", "famfamfam-silk"}},
    {"ExplorerShowHiddenServices", {"Show Hidden Services", "Show hidden services in Explorer", false}},
    {"RenderingApi", {"Rendering API", "Preferred rendering backend", QVariant::fromValue(Engine::Rendering::RenderApi::Auto)}},
    {"MSAA", {"MSAA", "Multisample anti-aliasing sample count", QVariant::fromValue(Engine::Enums::MSAA::Off)}},
    {"SurfaceMipmapping", {"Surface Mipmapping", "Mipmapped filtering for surface textures", QVariant::fromValue(Engine::Enums::SurfaceMipmapping::On)}},
    {"GizmoAlwaysOnTop", {"Gizmo Always On Top", "Render gizmos on top of scene geometry", true}},
    {"GizmoIgnoreDiffuseSpecular", {"Gizmo Ignore Lighting", "Ignore diffuse and specular lighting for gizmos", true}},
    {"GizmoAlignByMagnitude", {"Gizmo Align By Magnitude", "Place gizmo handles using bounds magnitude", true}},
    {"TransformSnapIncrement", {"Transform Snap Increment", "Snap amount for drag/move/size transforms (0 disables snap)", 1.0}},
    {"DockLayoutState", {"Dock Layout State", "Serialized dock and toolbar layout", ""}},
    {"Shortcut.Tool.Select", {"Select Tool", "Keyboard shortcut(s) for Select tool (separate with ';')", "1;Shift+1"}},
    {"Shortcut.Tool.Move", {"Move Tool", "Keyboard shortcut(s) for Move tool (separate with ';')", "2;Shift+2"}},
    {"Shortcut.Tool.Size", {"Size Tool", "Keyboard shortcut(s) for Size tool (separate with ';')", "3;Shift+3"}}
};

QMap<QString, QStringList> g_categories = {
    {"Studio",
        {
         "BaseCameraSpeed",
         "ShiftCameraSpeed",
	     "Theme",
	     "ExplorerIconPack",
	     "ExplorerShowHiddenServices",
	     "GizmoAlwaysOnTop",
	     "GizmoIgnoreDiffuseSpecular",
	     "GizmoAlignByMagnitude",
	     "TransformSnapIncrement"
        }
    },
    {"Rendering",
        {
         "RenderingApi",
         "MSAA",
         "SurfaceMipmapping"
        }
    },
    {"Shortcuts",
        {
         "Shortcut.Tool.Select",
         "Shortcut.Tool.Move",
         "Shortcut.Tool.Size"
        }
    }
};

QStringList g_categoryOrder = {"Studio", "Rendering", "Shortcuts"};

QMap<QString, QVariant> g_values;
QMap<QString, std::vector<std::pair<std::size_t, SettingChangedCallback>>> g_callbacks;
std::size_t g_nextConnectionId = 1;

void DisconnectListener(const QString& key, const std::size_t id) {
    auto it = g_callbacks.find(key);
    if (it == g_callbacks.end()) {
        return;
    }
    auto& listeners = it.value();
    listeners.erase(
        std::remove_if(
            listeners.begin(),
            listeners.end(),
            [id](const auto& item) { return item.first == id; }
        ),
        listeners.end()
    );
}

void FireChanged(const QString& key, const QVariant& value) {
    const auto listeners = g_callbacks.value(key);
    for (const auto& [id, callback] : listeners) {
        static_cast<void>(id);
        callback(value);
    }
}

QVariant ParseByDefaultType(const QVariant& defaultValue, const QJsonValue& value) {
    if (Engine::Enums::Metadata::IsRegisteredEnumType(defaultValue.typeId())) {
        const int typeId = defaultValue.typeId();
        if (value.isString()) {
            const QVariant parsed = Engine::Enums::Metadata::VariantFromName(typeId, value.toString());
            return parsed.isValid() ? parsed : defaultValue;
        }
        if (value.isDouble()) {
            const int fallback = Engine::Enums::Metadata::IntFromVariant(defaultValue);
            return Engine::Enums::Metadata::VariantFromInt(typeId, value.toInt(fallback));
        }
        return defaultValue;
    }

    switch (defaultValue.typeId()) {
        case QMetaType::Bool:
            return value.toBool(defaultValue.toBool());
        case QMetaType::Double:
            return value.toDouble(defaultValue.toDouble());
        case QMetaType::Int:
            return value.toInt(defaultValue.toInt());
        case QMetaType::QString:
            return value.toString(defaultValue.toString());
        default:
            return defaultValue;
    }
}

QJsonValue ToJsonValue(const QVariant& value) {
    if (Engine::Enums::Metadata::IsRegisteredEnumType(value.typeId())) {
        const QString name = Engine::Enums::Metadata::NameFromVariant(value);
        if (!name.isEmpty()) {
            return QJsonValue(name);
        }
        return QJsonValue(Engine::Enums::Metadata::IntFromVariant(value));
    }

    switch (value.typeId()) {
        case QMetaType::Bool:
            return QJsonValue(value.toBool());
        case QMetaType::Double:
            return QJsonValue(value.toDouble());
        case QMetaType::Int:
            return QJsonValue(value.toInt());
        case QMetaType::QString:
            return QJsonValue(value.toString());
        default:
            return QJsonValue(value.toString());
    }
}

void ApplyDefaults() {
    for (auto it = g_settings.cbegin(); it != g_settings.cend(); ++it) {
        if (!g_values.contains(it.key())) {
            g_values.insert(it.key(), it.value().DefaultValue);
        }
    }
}

void Verify() {
    for (auto it = g_settings.cbegin(); it != g_settings.cend(); ++it) {
        const auto& key = it.key();
        const auto& meta = it.value();
        if (!g_values.contains(key)) {
            g_values[key] = meta.DefaultValue;
            continue;
        }

	    QVariant value = g_values.value(key);
	    if (Engine::Enums::Metadata::IsRegisteredEnumType(meta.DefaultValue.typeId())) {
	        const QVariant coerced = Engine::Enums::Metadata::CoerceVariant(meta.DefaultValue.typeId(), value);
	        g_values[key] = coerced.isValid() ? coerced : meta.DefaultValue;
	        continue;
	    }
	    if (!value.convert(meta.DefaultValue.metaType())) {
	        g_values[key] = meta.DefaultValue;
	    } else {
	        g_values[key] = value;
	    }
	}
}

QString ResolveConfigPath() {
    return Engine::Utils::SourcePath::GetSourcePath("studioConfig.json");
}

} // namespace

Connection::Connection(QString key, const std::size_t id)
    : key_(std::move(key)),
      id_(id),
      connected_(true) {
}

void Connection::Disconnect() {
    if (!connected_) {
        return;
    }
    DisconnectListener(key_, id_);
    connected_ = false;
}

void Load() {
    const QString path = ConfigFilePath();
    QFile file(path);

    if (!file.exists()) {
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            throw std::runtime_error("Unable to create studioConfig.json");
        }
        file.write("{}");
        file.close();
    }

    if (!file.open(QIODevice::ReadOnly)) {
        throw std::runtime_error("Unable to open studioConfig.json");
    }

    const QByteArray raw = file.readAll();
    file.close();

    g_values.clear();

    const auto doc = QJsonDocument::fromJson(raw);
    const QJsonObject obj = doc.isObject() ? doc.object() : QJsonObject{};

    for (auto it = g_settings.cbegin(); it != g_settings.cend(); ++it) {
        const auto& key = it.key();
        const auto& meta = it.value();
        if (obj.contains(key)) {
            g_values[key] = ParseByDefaultType(meta.DefaultValue, obj.value(key));
        }
    }

    ApplyDefaults();
    Verify();
    Save();
}

void Save() {
    QJsonObject obj;
    for (auto it = g_settings.cbegin(); it != g_settings.cend(); ++it) {
        const QString& key = it.key();
        obj.insert(key, ToJsonValue(g_values.value(key, it.value().DefaultValue)));
    }

    QFile file(ConfigFilePath());
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        throw std::runtime_error("Unable to write studioConfig.json");
    }
    file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    file.close();
}

QVariant Get(const QString& key) {
    if (!g_settings.contains(key)) {
        throw std::runtime_error(QString("Unknown setting: %1").arg(key).toStdString());
    }
    return g_values.value(key, g_settings.value(key).DefaultValue);
}

bool Set(const QString& key, const QVariant& value) {
    if (!g_settings.contains(key)) {
        throw std::runtime_error(QString("Unknown setting: %1").arg(key).toStdString());
    }

    const QVariant& defaultValue = g_settings.value(key).DefaultValue;
    QVariant converted = value;
    if (Engine::Enums::Metadata::IsRegisteredEnumType(defaultValue.typeId())) {
        if (converted.typeId() != defaultValue.typeId()) {
            converted = Engine::Enums::Metadata::CoerceVariant(defaultValue.typeId(), converted);
        }
        if (!converted.isValid() || converted.typeId() != defaultValue.typeId()) {
            return false;
        }
    } else if (!converted.convert(defaultValue.metaType())) {
        return false;
    }

    if (g_values.value(key) == converted) {
        return true;
    }

    g_values[key] = converted;
    Save();
    FireChanged(key, converted);
    return true;
}

void Reset(const QString& key) {
    Set(key, GetDefault(key));
}

void ResetAll() {
    QStringList changedKeys;
    for (auto it = g_settings.cbegin(); it != g_settings.cend(); ++it) {
        if (g_values.value(it.key()) != it.value().DefaultValue) {
            changedKeys.push_back(it.key());
            g_values[it.key()] = it.value().DefaultValue;
        }
    }
    Save();
    for (const auto& key : changedKeys) {
        FireChanged(key, g_values.value(key));
    }
}

QVariant GetDefault(const QString& key) {
    if (!g_settings.contains(key)) {
        throw std::runtime_error(QString("Unknown setting: %1").arg(key).toStdString());
    }
    return g_settings.value(key).DefaultValue;
}

QString ConfigFilePath() {
    static const QString path = ResolveConfigPath();
    return path;
}

Connection Changed(const QString& key, SettingChangedCallback callback, const bool fireNow) {
    const std::size_t id = g_nextConnectionId++;
    g_callbacks[key].push_back({id, std::move(callback)});

    if (fireNow) {
        g_callbacks[key].back().second(Get(key));
    }

    return Connection(key, id);
}

const QMap<QString, SettingMeta>& All() {
    return g_settings;
}

const QMap<QString, QStringList>& Categories() {
    return g_categories;
}

const QStringList& CategoryOrder() {
    return g_categoryOrder;
}

} // namespace Lvs::Studio::Core::Settings
