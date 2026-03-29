#include "Lvs/Studio/Core/SessionLog.hpp"

#include "Lvs/Qt/QtBridge.hpp"
#include "Lvs/Engine/Utils/EngineDataPaths.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QMutex>
#include <QStringConverter>
#include <QTextStream>

#include <algorithm>
#include <vector>

namespace Lvs::Engine::Core::SessionLog {

namespace {

inline constexpr int MAX_SESSION_LOGS = 100;

QMutex g_mutex;
QFile g_file;
QString g_path;

QString DefaultSessionName() {
    const auto* app = QCoreApplication::instance();
    if (app != nullptr) {
        const QString name = app->applicationName();
        if (!name.isEmpty()) {
            return name;
        }
        const QString display = QGuiApplication::applicationDisplayName();
        if (!display.isEmpty()) {
            return display;
        }
    }
    return "Lvs";
}

QString SanitizeFileComponent(QString s) {
    s = s.trimmed();
    if (s.isEmpty()) {
        return "Lvs";
    }
    static const QChar underscore = '_';
    for (QChar& ch : s) {
        if (!(ch.isLetterOrNumber() || ch == '_' || ch == '-' || ch == '.')) {
            ch = underscore;
        }
    }
    return s;
}

QString MakeLogFilePath(const QString& sessionName) {
    const QString safe = SanitizeFileComponent(sessionName);
    const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const qint64 pid = QCoreApplication::applicationPid();
    return QDir(QtBridge::ToQString(Utils::EngineDataPaths::LogsDir())).filePath(QString("%1_%2_%3.log").arg(safe, ts).arg(pid));
}

void PruneOldLogsLocked(const QString& keepAbsolutePath) {
    const QString logsDirPath = QtBridge::ToQString(Utils::EngineDataPaths::LogsDir());
    QDir logsDir(logsDirPath);
    const QFileInfoList files = logsDir.entryInfoList(QStringList() << "*.log", QDir::Files | QDir::NoSymLinks);

    if (files.size() <= MAX_SESSION_LOGS) {
        return;
    }

    std::vector<QFileInfo> candidates;
    candidates.reserve(static_cast<std::size_t>(files.size()));
    int total = 0;

    const QString keep = QDir::cleanPath(QFileInfo(keepAbsolutePath).absoluteFilePath());

    for (const QFileInfo& fi : files) {
        ++total;
        const QString abs = QDir::cleanPath(fi.absoluteFilePath());
        if (abs == keep) {
            continue;
        }
        candidates.push_back(fi);
    }

    std::sort(
        candidates.begin(),
        candidates.end(),
        [](const QFileInfo& a, const QFileInfo& b) { return a.lastModified() < b.lastModified(); }
    );

    std::size_t idx = 0;
    while (total > MAX_SESSION_LOGS && idx < candidates.size()) {
        QFile::remove(candidates[idx].absoluteFilePath());
        --total;
        ++idx;
    }
}

void EnsureOpenLocked(const QString& sessionName) {
    if (g_file.isOpen()) {
        return;
    }
    g_path = MakeLogFilePath(sessionName.isEmpty() ? DefaultSessionName() : sessionName);
    g_file.setFileName(g_path);
    QDir().mkpath(QFileInfo(g_path).absolutePath());
    static_cast<void>(g_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text));
    PruneOldLogsLocked(g_path);
}

void WriteLineLocked(const QString& level, const QString& message) {
    EnsureOpenLocked({});
    if (!g_file.isOpen()) {
        return;
    }
    QTextStream stream(&g_file);
    stream.setEncoding(QStringConverter::Utf8);
    const QString ts = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss.zzz");
    stream << ts << " [" << level << "] " << message << "\n";
    stream.flush();
}

} // namespace

void Start(const QString& sessionName) {
    QMutexLocker lock(&g_mutex);
    EnsureOpenLocked(sessionName);
}

void Stop() {
    QMutexLocker lock(&g_mutex);
    if (g_file.isOpen()) {
        g_file.flush();
        g_file.close();
    }
}

void Write(const QString& message) {
    QMutexLocker lock(&g_mutex);
    WriteLineLocked("LOG", message);
}

void Info(const QString& message) {
    QMutexLocker lock(&g_mutex);
    WriteLineLocked("INFO", message);
}

void Warn(const QString& message) {
    QMutexLocker lock(&g_mutex);
    WriteLineLocked("WARN", message);
}

void Error(const QString& message) {
    QMutexLocker lock(&g_mutex);
    WriteLineLocked("ERROR", message);
}

QString CurrentFilePath() {
    QMutexLocker lock(&g_mutex);
    return g_path;
}

} // namespace Lvs::Engine::Core::SessionLog
