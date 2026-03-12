#pragma once

#include "Lvs/Engine/Enums/MeshCullMode.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"
#include "Lvs/Engine/Rendering/Common/SkyboxSettingsSnapshot.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"
#include "Lvs/Engine/Rendering/RHI/Types.hpp"

#include <array>
#include <cstddef>
#include <filesystem>
#include <string>

namespace Lvs::Engine::Rendering::Context {

[[nodiscard]] std::array<float, 16> ToFloatMat4ColumnMajor(const Math::Matrix4& matrix);
[[nodiscard]] Math::Matrix4 ApplyVulkanProjectionFlip(const Math::Matrix4& projection);
[[nodiscard]] Math::Matrix4 ApplyOpenGLShadowDepthRemap(const Math::Matrix4& clipZeroToOne);
[[nodiscard]] std::array<float, 4> ToVec4(const Math::Vector3& value, float w = 0.0F);
[[nodiscard]] std::array<float, 4> ToVec4(const Math::Color3& value, float w = 1.0F);

[[nodiscard]] RenderApi ResolveApi(RenderApi preferred);
[[nodiscard]] std::filesystem::path ResolveContentPath(const std::string& contentId);
[[nodiscard]] RHI::CullMode ToRhiCullMode(Enums::MeshCullMode mode);
[[nodiscard]] std::size_t BuildSkyboxSettingsKey(const Common::SkyboxSettingsSnapshot& snapshot);
[[nodiscard]] RHI::u32 ComputePostBlurLevels(float blurAmount);

} // namespace Lvs::Engine::Rendering::Context

