#pragma once
#include <any>
#include <chrono>
#include <exception>
#include <functional>
#include <iostream>
#include <map>
#include <memory>
#include <thread>
#include <typeindex>
#include <unordered_map>

namespace afv_native::event {
    using HandlerIdType = std::size_t;

    template <typename T>
    class EventStream {
      public:
        using CallbackType = std::function<void(const T &)>;

        HandlerIdType AddHandler(const CallbackType &callback) {
            HandlerIdType id = nextHandlerId_++;
            handlers_[id]    = callback;
            return id;
        }

        void OnEvent(const T &event) {
            for (const auto &[id, handler]: handlers_) {
                try {
                    handler(event);
                } catch (const std::exception &e) {
                    std::cerr << "Exception in handler #" << id << ": " << e.what() << "\n";
                }
            }
        }

      private:
        std::unordered_map<HandlerIdType, CallbackType> handlers_;
        HandlerIdType                                   nextHandlerId_ = 0;
    };

    class EventBus {
      public:
        static EventBus &Instance() {
            static EventBus instance;
            return instance;
        }

        EventBus(const EventBus &)            = delete;
        EventBus(EventBus &&)                 = delete;
        EventBus &operator=(const EventBus &) = delete;
        EventBus &operator=(EventBus &&)      = delete;

        template <typename T>
        HandlerIdType AddHandler(const std::function<void(const T &)> &callback) {
            return GetStream<T>().AddHandler(callback);
        }

        template <typename T>
        void OnEvent(const T &event) {
            GetStream<T>().OnEvent(event);
        }

      private:
        EventBus() = default;

        template <typename T>
        EventStream<T> &GetStream() {
            auto typeIdx = std::type_index(typeid(T));
            if (streams_.find(typeIdx) == streams_.end()) {
                streams_[typeIdx] = std::make_shared<EventStream<T>>();
            }
            return *std::any_cast<std::shared_ptr<EventStream<T>>>(streams_[typeIdx]);
        }

        std::map<std::type_index, std::any> streams_;
    };
} // namespace afv_native::event
