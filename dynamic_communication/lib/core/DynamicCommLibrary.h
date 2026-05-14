#pragma once

#include <vsomeip/vsomeip.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <vector>

namespace dynamic_comm {

template <typename T>
struct Serializer {
    static std::vector<vsomeip::byte_t> serialize(const T &) {
        static_assert(sizeof(T) == 0, "Serializer specialization is required");
        return {};
    }

    static T deserialize(const std::vector<vsomeip::byte_t> &) {
        static_assert(sizeof(T) == 0, "Serializer specialization is required");
        return T {};
    }
};

template <>
struct Serializer<uint32_t> {
    static std::vector<vsomeip::byte_t> serialize(const uint32_t &value) {
        return {
            static_cast<vsomeip::byte_t>((value >> 24) & 0xFF),
            static_cast<vsomeip::byte_t>((value >> 16) & 0xFF),
            static_cast<vsomeip::byte_t>((value >> 8) & 0xFF),
            static_cast<vsomeip::byte_t>(value & 0xFF)};
    }

    static uint32_t deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        if (bytes.size() != 4) {
            throw std::runtime_error("uint32_t payload must contain 4 bytes");
        }
        return (static_cast<uint32_t>(bytes[0]) << 24) |
               (static_cast<uint32_t>(bytes[1]) << 16) |
               (static_cast<uint32_t>(bytes[2]) << 8) |
               static_cast<uint32_t>(bytes[3]);
    }
};

namespace detail {

inline void append_u32_be(std::vector<vsomeip::byte_t> &out, uint32_t v) {
    out.push_back(static_cast<vsomeip::byte_t>((v >> 24) & 0xFF));
    out.push_back(static_cast<vsomeip::byte_t>((v >> 16) & 0xFF));
    out.push_back(static_cast<vsomeip::byte_t>((v >> 8) & 0xFF));
    out.push_back(static_cast<vsomeip::byte_t>(v & 0xFF));
}

inline uint32_t read_u32_be(const std::vector<vsomeip::byte_t> &bytes, std::size_t &offset) {
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("Malformed payload: missing uint32");
    }
    uint32_t v = (static_cast<uint32_t>(bytes[offset]) << 24) |
                 (static_cast<uint32_t>(bytes[offset + 1]) << 16) |
                 (static_cast<uint32_t>(bytes[offset + 2]) << 8) |
                 static_cast<uint32_t>(bytes[offset + 3]);
    offset += 4;
    return v;
}

inline void append_with_size_prefix(std::vector<vsomeip::byte_t> &out, const std::vector<vsomeip::byte_t> &chunk) {
    if (chunk.size() > 0xFFFFFFFFu) {
        throw std::runtime_error("Payload chunk too large");
    }
    append_u32_be(out, static_cast<uint32_t>(chunk.size()));
    out.insert(out.end(), chunk.begin(), chunk.end());
}

inline std::vector<vsomeip::byte_t> read_sized_chunk(const std::vector<vsomeip::byte_t> &bytes, std::size_t &offset) {
    uint32_t size = read_u32_be(bytes, offset);
    if (offset + size > bytes.size()) {
        throw std::runtime_error("Malformed payload: invalid chunk size");
    }
    std::vector<vsomeip::byte_t> chunk(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                       bytes.begin() + static_cast<std::ptrdiff_t>(offset + size));
    offset += size;
    return chunk;
}

}  // namespace detail

template <>
struct Serializer<uint16_t> {
    static std::vector<vsomeip::byte_t> serialize(const uint16_t &value) {
        return {
            static_cast<vsomeip::byte_t>((value >> 8) & 0xFF),
            static_cast<vsomeip::byte_t>(value & 0xFF)};
    }

    static uint16_t deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        if (bytes.size() != 2) {
            throw std::runtime_error("uint16_t payload must contain 2 bytes");
        }
        return static_cast<uint16_t>((static_cast<uint16_t>(bytes[0]) << 8) |
                                     static_cast<uint16_t>(bytes[1]));
    }
};

template <>
struct Serializer<std::string> {
    static std::vector<vsomeip::byte_t> serialize(const std::string &value) {
        std::vector<vsomeip::byte_t> out;
        if (value.size() > 0xFFFFFFFFu) {
            throw std::runtime_error("string payload too large");
        }
        detail::append_u32_be(out, static_cast<uint32_t>(value.size()));
        out.insert(out.end(), value.begin(), value.end());
        return out;
    }

    static std::string deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        std::size_t offset = 0;
        uint32_t size = detail::read_u32_be(bytes, offset);
        if (offset + size != bytes.size()) {
            throw std::runtime_error("Malformed string payload length");
        }
        return std::string(bytes.begin() + static_cast<std::ptrdiff_t>(offset), bytes.end());
    }
};

template <typename T>
struct Serializer<std::vector<T>> {
    static std::vector<vsomeip::byte_t> serialize(const std::vector<T> &values) {
        std::vector<vsomeip::byte_t> out;
        if (values.size() > 0xFFFFFFFFu) {
            throw std::runtime_error("vector payload too large");
        }
        detail::append_u32_be(out, static_cast<uint32_t>(values.size()));
        for (const auto &item : values) {
            std::vector<vsomeip::byte_t> encoded = Serializer<T>::serialize(item);
            detail::append_with_size_prefix(out, encoded);
        }
        return out;
    }

    static std::vector<T> deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        std::size_t offset = 0;
        uint32_t count = detail::read_u32_be(bytes, offset);
        std::vector<T> out;
        out.reserve(count);
        for (uint32_t i = 0; i < count; ++i) {
            std::vector<vsomeip::byte_t> chunk = detail::read_sized_chunk(bytes, offset);
            out.push_back(Serializer<T>::deserialize(chunk));
        }
        if (offset != bytes.size()) {
            throw std::runtime_error("Malformed vector payload trailing bytes");
        }
        return out;
    }
};

template <typename T>
struct Serializer<std::set<T>> {
    static std::vector<vsomeip::byte_t> serialize(const std::set<T> &values) {
        std::vector<vsomeip::byte_t> out;
        if (values.size() > 0xFFFFFFFFu) {
            throw std::runtime_error("set payload too large");
        }
        detail::append_u32_be(out, static_cast<uint32_t>(values.size()));
        for (const auto &item : values) {
            std::vector<vsomeip::byte_t> encoded = Serializer<T>::serialize(item);
            detail::append_with_size_prefix(out, encoded);
        }
        return out;
    }

    static std::set<T> deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        std::size_t offset = 0;
        uint32_t count = detail::read_u32_be(bytes, offset);
        std::set<T> out;
        for (uint32_t i = 0; i < count; ++i) {
            std::vector<vsomeip::byte_t> chunk = detail::read_sized_chunk(bytes, offset);
            out.insert(Serializer<T>::deserialize(chunk));
        }
        if (offset != bytes.size()) {
            throw std::runtime_error("Malformed set payload trailing bytes");
        }
        return out;
    }
};

template <typename K, typename V>
struct Serializer<std::map<K, V>> {
    static std::vector<vsomeip::byte_t> serialize(const std::map<K, V> &values) {
        std::vector<vsomeip::byte_t> out;
        if (values.size() > 0xFFFFFFFFu) {
            throw std::runtime_error("map payload too large");
        }
        detail::append_u32_be(out, static_cast<uint32_t>(values.size()));
        for (const auto &entry : values) {
            std::vector<vsomeip::byte_t> key_bytes = Serializer<K>::serialize(entry.first);
            std::vector<vsomeip::byte_t> val_bytes = Serializer<V>::serialize(entry.second);
            detail::append_with_size_prefix(out, key_bytes);
            detail::append_with_size_prefix(out, val_bytes);
        }
        return out;
    }

    static std::map<K, V> deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        std::size_t offset = 0;
        uint32_t count = detail::read_u32_be(bytes, offset);
        std::map<K, V> out;
        for (uint32_t i = 0; i < count; ++i) {
            std::vector<vsomeip::byte_t> key_chunk = detail::read_sized_chunk(bytes, offset);
            std::vector<vsomeip::byte_t> val_chunk = detail::read_sized_chunk(bytes, offset);
            out[Serializer<K>::deserialize(key_chunk)] = Serializer<V>::deserialize(val_chunk);
        }
        if (offset != bytes.size()) {
            throw std::runtime_error("Malformed map payload trailing bytes");
        }
        return out;
    }
};

class ServiceEndpoint {
public:
    virtual ~ServiceEndpoint() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

class ClientEndpoint {
public:
    virtual ~ClientEndpoint() = default;
    virtual void start() = 0;
    virtual void stop() = 0;
};

template <typename TRequest, typename TResponse>
class IRequestHandler {
public:
    virtual ~IRequestHandler() = default;
    virtual TResponse handle_request(const TRequest &request) = 0;
};

template <typename TResponse>
class IResponseConsumer {
public:
    virtual ~IResponseConsumer() = default;
    virtual void on_response(const TResponse &response, vsomeip::return_code_e code) = 0;
};

template <typename TRequest>
class IFireAndForgetHandler {
public:
    virtual ~IFireAndForgetHandler() = default;
    virtual void handle_oneway(const TRequest &request) = 0;
};

template <typename TRequest, typename TResponse>
class DynamicService : public ServiceEndpoint {
public:
    DynamicService(const std::string &app_name,
                   vsomeip::service_t service_id,
                   vsomeip::instance_t instance_id,
                   vsomeip::method_t method_id,
                   std::shared_ptr<IRequestHandler<TRequest, TResponse>> handler)
        : app_name_(app_name),
          service_id_(service_id),
          instance_id_(instance_id),
          method_id_(method_id),
          handler_(std::move(handler)) {}

    void start() override {
        app_ = vsomeip::runtime::get()->create_application(app_name_);
        if (!app_ || !app_->init()) {
            throw std::runtime_error("Failed to initialize service application");
        }

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                app_->offer_service(service_id_, instance_id_);
            }
        });

        app_->register_message_handler(
            service_id_, instance_id_, method_id_,
            [this](const std::shared_ptr<vsomeip::message> &request) { on_request(request); });

        worker_ = std::thread([this]() { app_->start(); });
    }

    void stop() override {
        if (app_) {
            app_->stop_offer_service(service_id_, instance_id_);
            app_->stop();
        }
        if (worker_.joinable()) {
            worker_.join();
        }
    }

private:
    void on_request(const std::shared_ptr<vsomeip::message> &request) {
        auto in_payload = request->get_payload();
        std::vector<vsomeip::byte_t> bytes(
            in_payload->get_data(),
            in_payload->get_data() + in_payload->get_length());

        TRequest typed_request = Serializer<TRequest>::deserialize(bytes);
        TResponse typed_response = handler_->handle_request(typed_request);
        std::vector<vsomeip::byte_t> out_bytes = Serializer<TResponse>::serialize(typed_response);

        auto response = vsomeip::runtime::get()->create_response(request);
        auto out_payload = vsomeip::runtime::get()->create_payload();
        out_payload->set_data(out_bytes);
        response->set_payload(out_payload);
        app_->send(response);
    }

    std::string app_name_;
    vsomeip::service_t service_id_;
    vsomeip::instance_t instance_id_;
    vsomeip::method_t method_id_;
    std::shared_ptr<IRequestHandler<TRequest, TResponse>> handler_;
    std::shared_ptr<vsomeip::application> app_;
    std::thread worker_;
};

template <typename TRequest>
class DynamicFireAndForgetService : public ServiceEndpoint {
public:
    DynamicFireAndForgetService(const std::string &app_name,
                                vsomeip::service_t service_id,
                                vsomeip::instance_t instance_id,
                                vsomeip::method_t method_id,
                                std::shared_ptr<IFireAndForgetHandler<TRequest>> handler)
        : app_name_(app_name),
          service_id_(service_id),
          instance_id_(instance_id),
          method_id_(method_id),
          handler_(std::move(handler)) {}

    void start() override {
        app_ = vsomeip::runtime::get()->create_application(app_name_);
        if (!app_ || !app_->init()) {
            throw std::runtime_error("Failed to initialize fire-and-forget service application");
        }

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                app_->offer_service(service_id_, instance_id_);
            }
        });

        app_->register_message_handler(
            service_id_, instance_id_, method_id_,
            [this](const std::shared_ptr<vsomeip::message> &request) { on_oneway(request); });

        worker_ = std::thread([this]() { app_->start(); });
    }

    void stop() override {
        if (app_) {
            app_->stop_offer_service(service_id_, instance_id_);
            app_->stop();
        }
        if (worker_.joinable()) {
            worker_.join();
        }
    }

private:
    void on_oneway(const std::shared_ptr<vsomeip::message> &request) {
        auto in_payload = request->get_payload();
        std::vector<vsomeip::byte_t> bytes(
            in_payload->get_data(),
            in_payload->get_data() + in_payload->get_length());

        TRequest typed_request = Serializer<TRequest>::deserialize(bytes);
        handler_->handle_oneway(typed_request);
    }

    std::string app_name_;
    vsomeip::service_t service_id_;
    vsomeip::instance_t instance_id_;
    vsomeip::method_t method_id_;
    std::shared_ptr<IFireAndForgetHandler<TRequest>> handler_;
    std::shared_ptr<vsomeip::application> app_;
    std::thread worker_;
};

template <typename TRequest, typename TResponse>
class DynamicClient : public ClientEndpoint {
public:
    DynamicClient(const std::string &app_name,
                  vsomeip::service_t service_id,
                  vsomeip::instance_t instance_id,
                  vsomeip::method_t method_id,
                  std::shared_ptr<IResponseConsumer<TResponse>> consumer)
        : app_name_(app_name),
          service_id_(service_id),
          instance_id_(instance_id),
          method_id_(method_id),
          consumer_(std::move(consumer)),
          available_(false) {}

    void start() override {
        app_ = vsomeip::runtime::get()->create_application(app_name_);
        if (!app_ || !app_->init()) {
            throw std::runtime_error("Failed to initialize client application");
        }

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                app_->request_service(service_id_, instance_id_);
            }
        });

        app_->register_availability_handler(
            service_id_, instance_id_,
            [this](vsomeip::service_t, vsomeip::instance_t, bool is_available) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    available_ = is_available;
                }
                cv_.notify_all();
            });

        app_->register_message_handler(
            service_id_, instance_id_, method_id_,
            [this](const std::shared_ptr<vsomeip::message> &response) { on_response(response); });

        worker_ = std::thread([this]() { app_->start(); });
    }

    bool wait_for_service(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return available_.load(); });
    }

    void send_request(const TRequest &request) {
        auto someip_request = vsomeip::runtime::get()->create_request();
        someip_request->set_service(service_id_);
        someip_request->set_instance(instance_id_);
        someip_request->set_method(method_id_);

        std::vector<vsomeip::byte_t> bytes = Serializer<TRequest>::serialize(request);
        auto payload = vsomeip::runtime::get()->create_payload();
        payload->set_data(bytes);
        someip_request->set_payload(payload);

        app_->send(someip_request);
    }

    void stop() override {
        if (app_) {
            app_->release_service(service_id_, instance_id_);
            app_->stop();
        }
        if (worker_.joinable()) {
            worker_.join();
        }
    }

private:
    void on_response(const std::shared_ptr<vsomeip::message> &response) {
        auto payload = response->get_payload();
        std::vector<vsomeip::byte_t> bytes(
            payload->get_data(),
            payload->get_data() + payload->get_length());
        TResponse typed_response = Serializer<TResponse>::deserialize(bytes);
        consumer_->on_response(typed_response, response->get_return_code());
    }

    std::string app_name_;
    vsomeip::service_t service_id_;
    vsomeip::instance_t instance_id_;
    vsomeip::method_t method_id_;
    std::shared_ptr<IResponseConsumer<TResponse>> consumer_;
    std::shared_ptr<vsomeip::application> app_;
    std::thread worker_;
    std::atomic<bool> available_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

template <typename TRequest>
class DynamicFireAndForgetClient : public ClientEndpoint {
public:
    DynamicFireAndForgetClient(const std::string &app_name,
                               vsomeip::service_t service_id,
                               vsomeip::instance_t instance_id,
                               vsomeip::method_t method_id)
        : app_name_(app_name),
          service_id_(service_id),
          instance_id_(instance_id),
          method_id_(method_id),
          available_(false) {}

    void start() override {
        app_ = vsomeip::runtime::get()->create_application(app_name_);
        if (!app_ || !app_->init()) {
            throw std::runtime_error("Failed to initialize fire-and-forget client application");
        }

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                app_->request_service(service_id_, instance_id_);
            }
        });

        app_->register_availability_handler(
            service_id_, instance_id_,
            [this](vsomeip::service_t, vsomeip::instance_t, bool is_available) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    available_ = is_available;
                }
                cv_.notify_all();
            });

        worker_ = std::thread([this]() { app_->start(); });
    }

    bool wait_for_service(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return available_.load(); });
    }

    void send_fire_and_forget(const TRequest &request) {
        auto someip_request = vsomeip::runtime::get()->create_request();
        someip_request->set_service(service_id_);
        someip_request->set_instance(instance_id_);
        someip_request->set_method(method_id_);
        someip_request->set_message_type(vsomeip::message_type_e::MT_REQUEST_NO_RETURN);

        std::vector<vsomeip::byte_t> bytes = Serializer<TRequest>::serialize(request);
        auto payload = vsomeip::runtime::get()->create_payload();
        payload->set_data(bytes);
        someip_request->set_payload(payload);

        app_->send(someip_request);
    }

    void stop() override {
        if (app_) {
            app_->release_service(service_id_, instance_id_);
            app_->stop();
        }
        if (worker_.joinable()) {
            worker_.join();
        }
    }

private:
    std::string app_name_;
    vsomeip::service_t service_id_;
    vsomeip::instance_t instance_id_;
    vsomeip::method_t method_id_;
    std::shared_ptr<vsomeip::application> app_;
    std::thread worker_;
    std::atomic<bool> available_;
    std::mutex mutex_;
    std::condition_variable cv_;
};

}  // namespace dynamic_comm

