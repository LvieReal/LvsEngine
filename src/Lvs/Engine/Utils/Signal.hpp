#pragma once

#include <algorithm>
#include <cstddef>
#include <functional>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

namespace Lvs::Engine::Utils {

template <typename... Args>
class Signal {
public:
    struct Listener {
        std::size_t Id;
        std::function<void(Args...)> Callback;
    };

    struct State {
        std::vector<Listener> Listeners;
        std::size_t NextId{1};
        std::mutex Mutex;
    };

    class Connection {
    public:
        Connection() = default;
        Connection(const std::weak_ptr<State>& state, const std::size_t id)
            : state_(state), id_(id) {
        }

        void Disconnect() {
            const auto state = state_.lock();
            if (state == nullptr) {
                return;
            }
            std::scoped_lock lock(state->Mutex);
            state->Listeners.erase(
                std::remove_if(
                    state->Listeners.begin(),
                    state->Listeners.end(),
                    [id = id_](const Listener& listener) { return listener.Id == id; }
                ),
                state->Listeners.end()
            );
            state_.reset();
        }

    private:
        std::weak_ptr<State> state_;
        std::size_t id_{0};
    };

    Connection Connect(std::function<void(Args...)> callback) {
        const auto state = state_;
        std::scoped_lock lock(state->Mutex);
        const std::size_t id = state->NextId++;
        state->Listeners.push_back({id, std::move(callback)});
        return Connection(state, id);
    }

    void Fire(Args... args) const {
        const auto state = state_;
        std::vector<Listener> listenersSnapshot;
        {
            std::scoped_lock lock(state->Mutex);
            listenersSnapshot = state->Listeners;
        }

        for (const auto& snapshotListener : listenersSnapshot) {
            std::function<void(Args...)> callback;
            {
                std::scoped_lock lock(state->Mutex);
                const auto it = std::find_if(
                    state->Listeners.cbegin(),
                    state->Listeners.cend(),
                    [id = snapshotListener.Id](const Listener& current) { return current.Id == id; }
                );
                if (it == state->Listeners.cend()) {
                    continue;
                }
                callback = it->Callback;
            }
            callback(args...);
        }
    }

private:
    std::shared_ptr<State> state_{std::make_shared<State>()};
};

} // namespace Lvs::Engine::Utils
