#include "Lvs/Studio/Core/Viewport.hpp"

#include "Lvs/Studio/Core/CriticalError.hpp"
#include "Lvs/Studio/Core/ViewportToolLayer.hpp"
#include "Lvs/Engine/Rendering/IRenderContext.hpp"
#include "Lvs/Engine/Utils/Benchmark.hpp"

#include <QCursor>
#include <QPaintEngine>
#include <QPaintEvent>
#include <QResizeEvent>
#include <Qt>

#include <algorithm>
#include <chrono>
#include <cstdint>

namespace Lvs::Engine::Core {

void Viewport::paintEvent(QPaintEvent* event) {
    LVS_BENCH_SCOPE("Viewport::paintEvent");
    static_cast<void>(event);
    if (graphicsUnavailable_) {
        return;
    }

    static auto previous = std::chrono::steady_clock::now();
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> delta = now - previous;
    previous = now;

    {
        LVS_BENCH_SCOPE("Viewport::UpdateCamera");
        UpdateCamera(delta.count());
    }

    std::optional<Utils::Ray> cursorRay;
    const QPoint local = mapFromGlobal(QCursor::pos());
    if (local.x() >= 0 && local.x() < width() && local.y() >= 0 && local.y() < height()) {
        {
            LVS_BENCH_SCOPE("Viewport::BuildRay");
            cursorRay = BuildRay(static_cast<double>(local.x()), static_cast<double>(local.y()));
        }
    }

    if (toolLayer_ != nullptr) {
        {
            LVS_BENCH_SCOPE("Viewport::ToolLayer::OnFrame");
            toolLayer_->OnFrame(delta.count(), cursorRay);
        }
    }

    if (context_ != nullptr && context_->RenderContext != nullptr) {
        std::vector<Rendering::Common::OverlayPrimitive> overlay;
        std::vector<Rendering::Common::Image3DPrimitive> images;
        if (toolLayer_ != nullptr) {
            {
                LVS_BENCH_SCOPE("Viewport::ToolLayer::AppendOverlay");
                toolLayer_->AppendOverlay(overlay);
            }
            {
                LVS_BENCH_SCOPE("Viewport::ToolLayer::AppendImage3D");
                toolLayer_->AppendImage3D(images);
            }
        }
        {
            LVS_BENCH_SCOPE("Viewport::RenderContext::SetOverlayPrimitives");
            context_->RenderContext->SetOverlayPrimitives(std::move(overlay));
        }
        {
            LVS_BENCH_SCOPE("Viewport::RenderContext::SetImage3DPrimitives");
            context_->RenderContext->SetImage3DPrimitives(std::move(images));
        }
    }

    try {
        EnsureGraphicsBound(); // keep out of bench scope noise; measured via RenderContext scopes below
        if (pendingResize_ && context_ != nullptr && context_->RenderContext != nullptr && graphicsBound_) {
            {
                LVS_BENCH_SCOPE("Viewport::RenderContext::Resize");
                context_->RenderContext->Resize(pendingResizeWidth_, pendingResizeHeight_);
            }
            pendingResize_ = false;
        }
        if (context_ != nullptr && context_->RenderContext != nullptr) {
            {
                LVS_BENCH_SCOPE("Viewport::RenderContext::Render");
                context_->RenderContext->Render();
            }
        }
    } catch (const Rendering::RenderingInitializationError& ex) {
        graphicsUnavailable_ = true;
        CriticalError::ShowGraphicsUnsupportedError(QString::fromUtf8(ex.what()));
        return;
    } catch (const std::exception& ex) {
        CriticalError::ShowUnexpectedNoReturnError(QString::fromUtf8(ex.what()));
    } catch (...) {
        CriticalError::ShowUnexpectedNoReturnError("Unknown error.");
    }

    if (isVisible()) {
        update();
    }
}

void Viewport::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);
    pendingResizeWidth_ = static_cast<std::uint32_t>(std::max(0, event->size().width()));
    pendingResizeHeight_ = static_cast<std::uint32_t>(std::max(0, event->size().height()));
    pendingResize_ = true;
}

QPaintEngine* Viewport::paintEngine() const {
    return nullptr;
}

void Viewport::EnsureGraphicsBound() {
    if (graphicsUnavailable_) {
        return;
    }
    if (graphicsBound_) {
        return;
    }
    if (context_ == nullptr || context_->RenderContext == nullptr) {
        return;
    }
    if (width() <= 0 || height() <= 0) {
        return;
    }

    const auto nativeHandle = reinterpret_cast<void*>(winId());
    context_->RenderContext->AttachToNativeWindow(
        nativeHandle,
        static_cast<std::uint32_t>(width()),
        static_cast<std::uint32_t>(height())
    );
    context_->RenderContext->SetClearColor(1.0F, 1.0F, 1.0F, 1.0F);
    graphicsBound_ = true;
}

void Viewport::RecreateRenderContext(const Rendering::RenderApi api) {
    if (context_ == nullptr) {
        return;
    }

    if (context_->RenderContext != nullptr) {
        context_->RenderContext->Unbind();
    }
    // Destroy old backend before recreating native window handle so GL/Vulkan
    // teardown can release resources against the original HWND.
    context_->RenderContext.reset();

    // OpenGL sets a pixel format on the viewport HDC that cannot be changed.
    // Recreate the native window to guarantee a fresh surface for Vulkan.
    if (testAttribute(Qt::WA_NativeWindow)) {
        const bool hadFocus = hasFocus();
        const bool wasVisible = isVisible();
        hide();
        destroy(true, true);
        setAttribute(Qt::WA_NativeWindow, true);
        setAttribute(Qt::WA_PaintOnScreen, true);
        setAttribute(Qt::WA_NoSystemBackground, true);
        setAttribute(Qt::WA_OpaquePaintEvent, true);
        // Force immediate native handle recreation.
        static_cast<void>(winId());
        if (wasVisible) {
            show();
        }
        if (hadFocus) {
            setFocus();
        }
    }

    context_->RenderContext = Rendering::CreateRenderContext(api);
    graphicsUnavailable_ = false;
    graphicsBound_ = false;

    if (context_->RenderContext == nullptr) {
        return;
    }

    if (const auto place = place_.lock(); place != nullptr) {
        context_->RenderContext->BindToPlace(place);
    }

    update();
}

} // namespace Lvs::Engine::Core
