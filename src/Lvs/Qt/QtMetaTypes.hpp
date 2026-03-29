#pragma once

#include "Lvs/Engine/Core/Instance.hpp"
#include "Lvs/Engine/Math/CFrame.hpp"
#include "Lvs/Engine/Math/Color3.hpp"
#include "Lvs/Engine/Math/Matrix4.hpp"
#include "Lvs/Engine/Math/Vector3.hpp"

#include <QMetaType>

#include <memory>

Q_DECLARE_METATYPE(std::shared_ptr<Lvs::Engine::Core::Instance>)
Q_DECLARE_METATYPE(Lvs::Engine::Math::Vector3)
Q_DECLARE_METATYPE(Lvs::Engine::Math::Color3)
Q_DECLARE_METATYPE(Lvs::Engine::Math::CFrame)
Q_DECLARE_METATYPE(Lvs::Engine::Math::Matrix4)

