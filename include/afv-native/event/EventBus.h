#pragma once
#include <functional>
#include <map>
#include <memory>
#include <shared_mutex>
#include <typeindex>
#include <unordered_map>
#include <iostream>
#include <vector>

namespace afv_native::event {
    using HandlerIdType = std::size_t;

    class IEventStream {
    public:
        virtual ~IEventStream() = default;
        virtual bool RemoveHandler(HandlerIdType id) = 0;
    };

    template <typename T>
    class EventStream : public IEventStream {
    public:
        using CallbackType = std::function<void(const T &)>;

        HandlerIdType AddHandler(const CallbackType &callback) {
            std::unique_lock lock(mutex_);
            HandlerIdType id = nextHandlerId_++;
            handlers_[id] = callback;
            return id;
        }

        bool RemoveHandler(HandlerIdType id) override {
            std::unique_lock lock(mutex_);
            return handlers_.erase(id) > 0;
        }

        void OnEvent(const T &event) {
            std::vector<std::pair<HandlerIdType, CallbackType>> snapshot;
            {
                std::shared_lock lock(mutex_);
                snapshot.assign(handlers_.begin(), handlers_.end());
            }
            for (const auto &[id, handler]: snapshot) {
                try {
                    handler(event);
                } catch (const std::exception &e) {
                    std::cerr << "Exception in handler #" << id << ": " << e.what() << "\n";
                }
            }
        }

    private:
        std::unordered_map<HandlerIdType, CallbackType> handlers_;
        std::shared_mutex mutex_;
        inline static HandlerIdType nextHandlerId_ = 0;
    };

    class EventBus {
    public:
        static EventBus &Instance() {
            static EventBus instance;
            return instance;
        }

        EventBus(const EventBus &) = delete;
        EventBus(EventBus &&) = delete;
        EventBus &operator=(const EventBus &) = delete;
        EventBus &operator=(EventBus &&) = delete;

        template <typename T>
        HandlerIdType AddHandler(const std::function<void(const T &)> &callback) {
            std::unique_lock lock(mutex_);
            auto id = GetStream<T>().AddHandler(callback);
            handlerTypes_.insert_or_assign(id, std::type_index(typeid(T)));
            return id;
        }

        bool RemoveHandler(HandlerIdType id) {
            std::unique_lock lock(mutex_);
            auto it = handlerTypes_.find(id);
            if (it == handlerTypes_.end()) {
                return false;
            }

            auto typeIdx = it->second;
            auto streamIt = streams_.find(typeIdx);

            if (streamIt != streams_.end()) {
                bool removed = streamIt->second->RemoveHandler(id);
                if (removed) {
                    handlerTypes_.erase(it);
                }
                return removed;
            }

            return false;
        }

        template <typename T>
        void OnEvent(const T &event) {
            // Get the stream pointer under lock, then release before
            // dispatching. Handlers may call AddHandler/RemoveHandler,
            // which need exclusive access to mutex_.
            std::shared_ptr<IEventStream> stream;
            {
                std::shared_lock lock(mutex_);
                auto typeIdx = std::type_index(typeid(T));
                auto it = streams_.find(typeIdx);
                if (it == streams_.end()) {
                    return;
                }
                stream = it->second;
            }
            static_cast<EventStream<T>*>(stream.get())->OnEvent(event);
        }

        void Reset() {
            std::unique_lock lock(mutex_);
            streams_.clear();
            handlerTypes_.clear();
        }

    private:
        EventBus() = default;

        template <typename T>
        EventStream<T> &GetStream() {
            auto typeIdx = std::type_index(typeid(T));
            if (streams_.find(typeIdx) == streams_.end()) {
                streams_[typeIdx] = std::make_shared<EventStream<T>>();
            }
            return *static_cast<EventStream<T>*>(streams_[typeIdx].get());
        }

        std::map<std::type_index, std::shared_ptr<IEventStream>> streams_;
        std::unordered_map<HandlerIdType, std::type_index> handlerTypes_;
        std::shared_mutex mutex_;
    };
} // namespace afv_native::event
