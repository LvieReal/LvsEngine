#pragma once

#include <map>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Lvs::Engine::Core {

using String = std::string;
using StringView = std::string_view;

template <class K, class V>
using HashMap = std::unordered_map<K, V>;

template <class K, class V>
using OrderedMap = std::map<K, V>;

template <class T>
using Vector = std::vector<T>;

using StringList = std::vector<String>;

} // namespace Lvs::Engine::Core

