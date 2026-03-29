#include "Lvs/Studio/Core/CrashHandler.hpp"

#include "Lvs/Studio/Core/SessionLog.hpp"
#include "Lvs/Qt/QtBridge.hpp"
#include "Lvs/Engine/Utils/EngineDataPaths.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>

#include <atomic>
#include <csignal>
#include <cstdlib>
#include <exception>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace Lvs::Engine::Core::CrashHandler {

namespace {

std::atomic_bool g_installed{false};

QString MakeCrashFilePath() {
    const QString ts = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    const qint64 pid = QCoreApplication::applicationPid();
    return QDir(QtBridge::ToQString(Utils::EngineDataPaths::CrashLogsDir())).filePath(QString("crash_%1_%2.txt").arg(ts).arg(pid));
}

QString AppLabel() {
    const auto* app = QCoreApplication::instance();
    if (app == nullptr) {
        return "Lvs";
    }
    const QString name = app->applicationName();
    if (!name.isEmpty()) {
        return name;
    }
    const QString display = QGuiApplication::applicationDisplayName();
    return display.isEmpty() ? "Lvs" : display;
}

void WriteCrashFileQt(const QString& title, const QString& details) {
    const QString path = MakeCrashFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        return;
    }

    const QString ts = QDateTime::currentDateTime().toString(Qt::ISODateWithMs);
    const qint64 pid = QCoreApplication::applicationPid();

    file.write(QString("App: %1\n").arg(AppLabel()).toUtf8());
    file.write(QString("Timestamp: %1\n").arg(ts).toUtf8());
    file.write(QString("PID: %1\n").arg(pid).toUtf8());
    file.write(QString("Title: %1\n").arg(title).toUtf8());
    if (!details.trimmed().isEmpty()) {
        file.write("\nDetails:\n");
        file.write(details.toUtf8());
        if (!details.endsWith('\n')) {
            file.write("\n");
        }
    }
    file.close();
}

[[noreturn]] void TerminateHandler() {
    QString title = "std::terminate";
    QString details;

    try {
        auto eptr = std::current_exception();
        if (eptr) {
            try {
                std::rethrow_exception(eptr);
            } catch (const std::exception& ex) {
                details = QString("Unhandled exception: %1").arg(QString::fromUtf8(ex.what()));
            } catch (...) {
                details = "Unhandled non-std exception";
            }
        }
    } catch (...) {
    }

    SessionLog::Error(title + (details.isEmpty() ? "" : (": " + details)));
    WriteCrashFileQt(title, details);
    std::abort();
}

#if defined(_WIN32)
void WriteWinCrashLog(EXCEPTION_POINTERS* info);
#endif

void SignalHandler(int sig) {
    const QString title = QString("Signal %1").arg(sig);
#if defined(_WIN32)
    static_cast<void>(title);
    WriteWinCrashLog(nullptr);
#else
    SessionLog::Error(title);
    WriteCrashFileQt(title, {});
#endif
    std::_Exit(128 + sig);
}

#if defined(_WIN32)
std::wstring EnsureWinDir(const std::wstring& path) {
    if (path.empty()) {
        return path;
    }
    std::wstring cur;
    cur.reserve(path.size());
    for (std::size_t i = 0; i < path.size(); ++i) {
        const wchar_t ch = path[i];
        cur.push_back(ch);
        if (ch == L'\\' || ch == L'/') {
            if (cur.size() > 3) {
                CreateDirectoryW(cur.c_str(), nullptr);
            }
        }
    }
    CreateDirectoryW(cur.c_str(), nullptr);
    return path;
}

std::wstring WinCrashDir() {
    wchar_t buffer[4096] = {};
    DWORD len = GetEnvironmentVariableW(L"LOCALAPPDATA", buffer, static_cast<DWORD>(std::size(buffer)));
    std::wstring base;
    if (len == 0 || len >= std::size(buffer)) {
        len = GetEnvironmentVariableW(L"USERPROFILE", buffer, static_cast<DWORD>(std::size(buffer)));
        if (len == 0 || len >= std::size(buffer)) {
            return L"";
        }
        base.assign(buffer, buffer + len);
        base += L"\\AppData\\Local";
    } else {
        base.assign(buffer, buffer + len);
    }

    std::wstring dir = base + L"\\LvsEngine\\crash_logs";
    EnsureWinDir(dir);
    return dir;
}

std::wstring WinCrashFilePath() {
    SYSTEMTIME st;
    GetLocalTime(&st);
    DWORD pid = GetCurrentProcessId();

    wchar_t name[256] = {};
    swprintf_s(
        name,
        L"crash_%04u%02u%02u_%02u%02u%02u_%03u_%lu.txt",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond,
        st.wMilliseconds,
        static_cast<unsigned long>(pid)
    );

    const std::wstring dir = WinCrashDir();
    if (dir.empty()) {
        return name;
    }
    return dir + L"\\" + name;
}

void WriteWinCrashLog(EXCEPTION_POINTERS* info) {
    const std::wstring path = WinCrashFilePath();
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) {
        return;
    }

    SYSTEMTIME st;
    GetLocalTime(&st);
    const DWORD tid = GetCurrentThreadId();
    const DWORD pid = GetCurrentProcessId();

    wchar_t header[1024] = {};
    const DWORD code = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionCode : 0;
    const void* addr = info && info->ExceptionRecord ? info->ExceptionRecord->ExceptionAddress : nullptr;

    swprintf_s(
        header,
        L"App: %s\r\nTimestamp: %04u-%02u-%02uT%02u:%02u:%02u.%03u\r\nPID: %lu\r\nTID: %lu\r\nExceptionCode: 0x%08lX\r\nExceptionAddress: 0x%p\r\n",
        L"Lvs",
        st.wYear,
        st.wMonth,
        st.wDay,
        st.wHour,
        st.wMinute,
        st.wSecond,
        st.wMilliseconds,
        static_cast<unsigned long>(pid),
        static_cast<unsigned long>(tid),
        static_cast<unsigned long>(code),
        addr
    );

    DWORD written = 0;
    WriteFile(file, header, static_cast<DWORD>(wcslen(header) * sizeof(wchar_t)), &written, nullptr);
    CloseHandle(file);
}

LONG WINAPI UnhandledExceptionFilterFn(EXCEPTION_POINTERS* info) {
    WriteWinCrashLog(info);
    return EXCEPTION_EXECUTE_HANDLER;
}
#endif

} // namespace

void Install() {
    bool expected = false;
    if (!g_installed.compare_exchange_strong(expected, true)) {
        return;
    }

    std::set_terminate(&TerminateHandler);
    std::signal(SIGSEGV, &SignalHandler);
    std::signal(SIGABRT, &SignalHandler);
    std::signal(SIGFPE, &SignalHandler);
    std::signal(SIGILL, &SignalHandler);

#if defined(_WIN32)
    SetUnhandledExceptionFilter(&UnhandledExceptionFilterFn);
#endif
}

void WriteCrashLog(const QString& title, const QString& details) {
    SessionLog::Error(title);
    WriteCrashFileQt(title, details);
}

void WriteCrashLogFromException(const std::exception& ex, const QString& context) {
    const QString title = context.isEmpty() ? "Unhandled exception" : context;
    WriteCrashLog(title, QString::fromUtf8(ex.what()));
}

} // namespace Lvs::Engine::Core::CrashHandler
