#include "Lvs/Studio/Core/Settings.hpp"
#include "Lvs/Engine/Utils/SourcePath.hpp"

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
    {"Theme", {"Theme", "Main studio theme", "Light"}},
    {"ExplorerIconPack", {"Explorer Icon Pack", "Explorer icon pack folder name", "famfamfam-silk"}},
    {"GizmoAlwaysOnTop", {"Gizmo Always On Top", "Render gizmos on top of scene geometry", true}},
    {"GizmoIgnoreDiffuseSpecular", {"Gizmo Ignore Lighting", "Ignore diffuse and specular lighting for gizmos", true}},
    {"GizmoAlignByMagnitude", {"Gizmo Align By Magnitude", "Place gizmo handles using bounds magnitude", true}},
    {"DockLayoutState", {"Dock Layout State", "Serialized dock and toolbar layout", ""}}
};

QMap<QString, QStringList> g_categories = {
    {"Studio",
        {"BaseCameraSpeed",
         "ShiftCameraSpeed",
         "Theme",
         "ExplorerIconPack",
         "GizmoAlwaysOnTop",
         "GizmoIgnoreDiffuseSpecular",
         "GizmoAlignByMagnitude"
        }
    }
};

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
        if (!value.convert(meta.DefaultValue.metaType())) {
            g_values[key] = meta.DefaultValue;
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

    QVariant converted = value;
    const QVariant& defaultValue = g_settings.value(key).DefaultValue;
    if (!converted.convert(defaultValue.metaType())) {
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

} // namespace Lvs::Studio::Core::Settings
