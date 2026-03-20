#pragma once

#include "Lvs/Engine/IO/IFileSystem.hpp"
#include "Lvs/Engine/IO/IXml.hpp"

#include <memory>

namespace Lvs::Engine::IO::Providers {

void SetFileSystem(std::shared_ptr<IFileSystem> fs);
void SetXml(std::shared_ptr<IXml> xml);

[[nodiscard]] std::shared_ptr<IFileSystem> GetFileSystem();
[[nodiscard]] std::shared_ptr<IXml> GetXml();

} // namespace Lvs::Engine::IO::Providers

