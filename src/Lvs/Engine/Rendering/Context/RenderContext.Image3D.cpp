#include "Lvs/Engine/Rendering/Context/RenderContext.hpp"

#include "Lvs/Engine/DataModel/Objects/Camera.hpp"
#include "Lvs/Engine/DataModel/Objects/Image3D.hpp"
#include "Lvs/Engine/DataModel/Place.hpp"
#include "Lvs/Engine/DataModel/Services/Workspace.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Rendering/Context/RenderContextUtils.hpp"
#include "Lvs/Engine/Rendering/Common/MeshData.hpp"
#include "Lvs/Engine/Utils/FileIO.hpp"
#include "Lvs/Engine/Utils/ImageIO.hpp"
#include "Lvs/Engine/Utils/PathUtils.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <sstream>

namespace Lvs::Engine::Rendering {

namespace {

std::filesystem::path ResolveImagePath(const std::string& contentId) {
    if (contentId.empty()) {
        return {};
    }

    const std::filesystem::path rawPath = std::filesystem::path(contentId);
    const std::array<std::filesystem::path, 6> candidates{
        rawPath,
        Utils::PathUtils::ToOsPath(contentId),
        Utils::PathUtils::GetResourcePath(contentId),
        Utils::PathUtils::GetSourcePath(contentId),
        Utils::PathUtils::GetSourcePath(std::string("content/") + contentId),
        Utils::PathUtils::GetResourcePath(std::string("IconPacks/") + contentId)
    };

    for (const auto& candidate : candidates) {
        if (candidate.empty()) {
            continue;
        }
        if (Utils::FileIO::Exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

float ComputeDistanceFade(const double distance, const double maxDistance) {
    if (!(maxDistance > 0.0)) {
        return 1.0F;
    }
    const double fadeStart = std::max(0.0, maxDistance * 0.85);
    if (distance <= fadeStart) {
        return 1.0F;
    }
    if (distance >= maxDistance) {
        return 0.0F;
    }
    const double t = (distance - fadeStart) / std::max(1e-9, (maxDistance - fadeStart));
    return static_cast<float>(std::clamp(1.0 - t, 0.0, 1.0));
}

} // namespace

RenderContext::GpuMesh* RenderContext::GetOrCreateQuadMesh() {
    if (quadMesh_.has_value()) {
        return &quadMesh_.value();
    }

    Common::MeshData mesh{};
    mesh.Vertices = {
        Common::VertexP3N3{{-0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        Common::VertexP3N3{{0.5F, -0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        Common::VertexP3N3{{-0.5F, 0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}},
        Common::VertexP3N3{{0.5F, 0.5F, 0.0F}, {0.0F, 0.0F, 1.0F}},
    };
    mesh.Indices = {0U, 1U, 2U, 2U, 1U, 3U};

    auto uploaded = CreateGpuMeshFromData(mesh);
    if (!uploaded.has_value()) {
        return nullptr;
    }
    quadMesh_ = std::move(uploaded.value());
    return &quadMesh_.value();
}

const RHI::IResourceSet* RenderContext::GetOrCreateImageTextureResources(const std::string& contentId, const int resolutionCap) {
    const auto resolvedPath = ResolveImagePath(contentId);
    if (resolvedPath.empty()) {
        return nullptr;
    }

    std::ostringstream oss;
    oss << resolvedPath.string() << "|cap=" << resolutionCap;
    const std::string key = oss.str();

    if (const auto it = imageTextureCache_.find(key); it != imageTextureCache_.end()) {
        return it->second.Resources.get();
    }

    Utils::ImageIO::ImageRgba8 image{};
    try {
        image = Utils::ImageIO::LoadRgba8(resolvedPath);
    } catch (const std::exception&) {
        return nullptr;
    }

    if (resolutionCap > 0) {
        const int maxSide = std::max(static_cast<int>(image.Width), static_cast<int>(image.Height));
        if (maxSide > resolutionCap) {
            image = Utils::ImageIO::ResizeRgba8(image, resolutionCap, resolutionCap, true, true);
        }
    }

    RHI::Texture2DDesc desc{};
    desc.width = image.Width;
    desc.height = image.Height;
    desc.format = RHI::Format::R8G8B8A8_UNorm;
    desc.linearFiltering = true;
    desc.generateMipmaps = true;
    desc.pixels = std::move(image.Pixels);

    auto texture = GetRhiContext().CreateTexture2D(desc);
    if (texture.graphic_handle_ptr == nullptr) {
        return nullptr;
    }

    const std::array<RHI::ResourceBinding, 1> bindings{
        RHI::ResourceBinding{
            .slot = 1,
            .kind = RHI::ResourceBindingKind::Texture2D,
            .texture = texture,
            .buffer = nullptr
        }
    };
    auto set = GetRhiContext().CreateResourceSet(RHI::ResourceSetDesc{
        .bindings = bindings.data(),
        .bindingCount = static_cast<RHI::u32>(bindings.size()),
        .nativeHandleHint = nullptr
    });
    if (set == nullptr) {
        GetRhiContext().DestroyTexture(texture);
        return nullptr;
    }

    ImageTextureEntry entry{};
    entry.Texture = texture;
    entry.Resources = std::move(set);
    entry.HasTexture = true;

    const RHI::IResourceSet* ptr = entry.Resources.get();
    imageTextureCache_.emplace(key, std::move(entry));
    return ptr;
}

std::vector<SceneData::Image3DDrawPacket> RenderContext::BuildImage3DDraws() {
    std::vector<SceneData::Image3DDrawPacket> draws{};
    if (place_ == nullptr) {
        return draws;
    }

    std::vector<Common::Image3DPrimitive> primitives = image3dPrimitives_;

    const auto workspaceService = std::dynamic_pointer_cast<DataModel::Workspace>(place_->FindService("Workspace"));
    if (workspaceService != nullptr) {
        workspaceService->ForEachDescendant([&](const std::shared_ptr<Core::Instance>& descendant) {
            const auto obj = std::dynamic_pointer_cast<DataModel::Objects::Image3D>(descendant);
            if (obj == nullptr || obj->GetParent() == nullptr) {
                return;
            }

            Common::Image3DPrimitive img{};
            img.Position = obj->GetProperty("Position").value<Math::Vector3>();
            img.Size = 1.0;
            img.Tint = obj->GetProperty("Tint").value<Math::Color3>();
            const double transparency = std::clamp(obj->GetProperty("Transparency").toDouble(), 0.0, 1.0);
            img.Alpha = static_cast<float>(std::clamp(1.0 - transparency, 0.0, 1.0));
            img.ContentId = obj->GetProperty("ContentId").toString();
            img.ResolutionCap = obj->GetProperty("ResolutionCap").toInt();
            img.FollowCamera = obj->GetProperty("FollowCamera").toBool();
            img.ConstantSize = obj->GetProperty("ConstantSize").toBool();
            img.MaxDistance = obj->GetProperty("MaxDistance").toDouble();
            img.AlwaysOnTop = obj->GetProperty("AlwaysOnTop").toBool();
            primitives.push_back(std::move(img));
        });
    }

    if (primitives.empty()) {
        return draws;
    }

    auto* quad = GetOrCreateQuadMesh();
    if (quad == nullptr) {
        return draws;
    }
    const SceneData::MeshRef* quadRef = GetOrCreateMeshRef("primitive:quad", *quad);
    if (quadRef == nullptr) {
        return draws;
    }

    std::shared_ptr<DataModel::Objects::Camera> camera{};
    if (workspaceService != nullptr) {
        const auto cameraVar = workspaceService->GetProperty("CurrentCamera");
        if (cameraVar.Is<Core::Variant::InstanceRef>()) {
            if (const auto locked = cameraVar.Get<Core::Variant::InstanceRef>().lock()) {
                camera = std::dynamic_pointer_cast<DataModel::Objects::Camera>(locked);
            }
        }
    }

    Math::CFrame cameraCFrame = Math::CFrame::Identity();
    if (camera != nullptr) {
        cameraCFrame = camera->GetProperty("CFrame").value<Math::CFrame>();
    }
    const Math::Vector3 camPos = cameraCFrame.Position;
    const Math::Vector3 camRight = cameraCFrame.RightVector().Unit();
    const Math::Vector3 camUp = cameraCFrame.UpVector().Unit();
    const Math::Vector3 camBack = cameraCFrame.Back.Unit();

    draws.reserve(primitives.size());
    for (const auto& img : primitives) {
        const RHI::IResourceSet* resources = GetOrCreateImageTextureResources(img.ContentId, img.ResolutionCap);
        if (resources == nullptr) {
            continue;
        }

        const double distance = (img.Position - camPos).Magnitude();
        const float fade = ComputeDistanceFade(distance, img.MaxDistance);
        const float alpha = std::clamp(img.Alpha * fade, 0.0F, 1.0F);
        if (alpha <= 0.001F) {
            continue;
        }

        const double distanceScale = img.ConstantSize ? std::max(0.1, distance / 10.0) : 1.0;
        const double size = std::max(0.001, img.Size) * distanceScale;

        Math::CFrame frame = Math::CFrame::New(img.Position);
        if (img.FollowCamera && camera != nullptr) {
            frame = Math::CFrame{img.Position, camRight, camUp, camBack};
        }

        const Math::Matrix4 model = frame.ToMatrix4() * Math::Matrix4::Scale({size, size, size});

        SceneData::Image3DDrawPacket packet{};
        packet.Mesh = quadRef;
        packet.TextureResources = resources;
        packet.Push.Model = Context::ToFloatMat4ColumnMajor(model);
        packet.Push.Color = Context::ToVec4(img.Tint, alpha);
        packet.SortDepth = static_cast<float>(distance * distance);
        packet.AlwaysOnTop = img.AlwaysOnTop;
        draws.push_back(packet);
    }

    return draws;
}

} // namespace Lvs::Engine::Rendering
