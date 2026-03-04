#pragma once

#include <QMap>
#include <QString>
#include <QStringList>
#include <QVariant>

#include <cstddef>
#include <functional>

namespace Lvs::Studio::Core::Settings {

struct SettingMeta {
    QString Name;
    QString Description;
    QVariant DefaultValue;
};

class Connection {
public:
    Connection() = default;
    Connection(QString key, std::size_t id);
    void Disconnect();

private:
    QString key_;
    std::size_t id_{0};
    bool connected_{false};
};

using SettingChangedCallback = std::function<void(const QVariant&)>;

void Load();
void Save();

QVariant Get(const QString& key);
bool Set(const QString& key, const QVariant& value);
void Reset(const QString& key);
void ResetAll();

QVariant GetDefault(const QString& key);
QString ConfigFilePath();
Connection Changed(const QString& key, SettingChangedCallback callback, bool fireNow = false);

const QMap<QString, SettingMeta>& All();
const QMap<QString, QStringList>& Categories();

} // namespace Lvs::Studio::Core::Settings
