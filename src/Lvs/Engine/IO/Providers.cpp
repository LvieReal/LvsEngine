#include "Lvs/Engine/IO/Providers.hpp"

#include <mutex>

namespace Lvs::Engine::IO::Providers {

namespace {
std::mutex g_mutex;
std::shared_ptr<IFileSystem> g_fs;
std::shared_ptr<IXml> g_xml;
} // namespace

void SetFileSystem(std::shared_ptr<IFileSystem> fs) {
    std::lock_guard lock(g_mutex);
    g_fs = std::move(fs);
}

void SetXml(std::shared_ptr<IXml> xml) {
    std::lock_guard lock(g_mutex);
    g_xml = std::move(xml);
}

std::shared_ptr<IFileSystem> GetFileSystem() {
    std::lock_guard lock(g_mutex);
    return g_fs;
}

std::shared_ptr<IXml> GetXml() {
    std::lock_guard lock(g_mutex);
    return g_xml;
}

} // namespace Lvs::Engine::IO::Providers

