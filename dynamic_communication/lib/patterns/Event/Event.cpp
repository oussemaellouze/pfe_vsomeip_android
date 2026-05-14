#include "patterns/Event/Event.h"

#include "core/DynamicCommLibrary.h"

#include "types/pubsub_types.hpp"

#include <vsomeip/vsomeip.hpp>

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <iostream>
#include <memory>
#include <mutex>
#include <thread>

namespace dynamic_comm {

class EventService : public ServiceEndpoint {
public:
    explicit EventService(const PatternRuntimeConfig &cfg) : cfg_(cfg) {}

    void start() override {
        app_ = vsomeip::runtime::get()->create_application(cfg_.app_service_name);
        if (!app_ || !app_->init()) throw std::runtime_error("Failed to init EventService");

        service_id_ = static_cast<vsomeip::service_t>(cfg_.service_id);
        instance_id_ = static_cast<vsomeip::instance_t>(cfg_.instance_id);
        event_id_ = static_cast<vsomeip::event_t>(cfg_.event_id);
        eventgroup_id_ = static_cast<vsomeip::eventgroup_t>(cfg_.eventgroup_id);

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                app_->offer_service(service_id_, instance_id_);
                app_->offer_event(service_id_, instance_id_, event_id_,
                                  {eventgroup_id_},
                                  vsomeip::event_type_e::ET_EVENT,
                                  std::chrono::milliseconds::zero(),
                                  false, true);
            }
        });

        running_ = true;
        worker_ = std::thread([this]() { app_->start(); });
        publisher_ = std::thread([this]() { publish_loop(); });
    }

    void stop() override {
        running_ = false;
        if (publisher_.joinable()) publisher_.join();
        if (app_) {
            app_->stop_offer_service(service_id_, instance_id_);
            app_->stop();
        }
        if (worker_.joinable()) worker_.join();
    }

private:
    void publish_loop() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        uint32_t counter = 0;
        while (running_) {
            if (cfg_.max_iterations > 0 && counter >= cfg_.max_iterations) {
                break;
            }
            VehicleEventPayload p {counter++};
            auto bytes = Serializer<VehicleEventPayload>::serialize(p);
            auto payload = vsomeip::runtime::get()->create_payload();
            payload->set_data(bytes);
            app_->notify(service_id_, instance_id_, event_id_, payload);
            std::cout << "[ECU EVENT] Notify event_id=0x" << std::hex << cfg_.event_id
                      << std::dec << " value=" << p.value << std::endl;
            std::this_thread::sleep_for(std::chrono::milliseconds(300));
        }
    }

    PatternRuntimeConfig cfg_;
    vsomeip::service_t service_id_ {};
    vsomeip::instance_t instance_id_ {};
    vsomeip::event_t event_id_ {};
    vsomeip::eventgroup_t eventgroup_id_ {};
    std::shared_ptr<vsomeip::application> app_;
    std::thread worker_;
    std::thread publisher_;
    std::atomic<bool> running_ {false};
};

class EventClient : public ClientEndpoint {
public:
    explicit EventClient(const PatternRuntimeConfig &cfg) : cfg_(cfg) {}

    void start() override {
        app_ = vsomeip::runtime::get()->create_application(cfg_.app_client_name);
        if (!app_ || !app_->init()) throw std::runtime_error("Failed to init EventClient");

        service_id_ = static_cast<vsomeip::service_t>(cfg_.service_id);
        instance_id_ = static_cast<vsomeip::instance_t>(cfg_.instance_id);
        event_id_ = static_cast<vsomeip::event_t>(cfg_.event_id);
        eventgroup_id_ = static_cast<vsomeip::eventgroup_t>(cfg_.eventgroup_id);

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                app_->request_service(service_id_, instance_id_);
            }
        });

        app_->register_availability_handler(service_id_, instance_id_,
            [this](vsomeip::service_t, vsomeip::instance_t, bool is_available) {
                {
                    std::lock_guard<std::mutex> lock(mutex_);
                    available_ = is_available;
                }
                cv_.notify_all();
            });

        app_->register_message_handler(service_id_, instance_id_, event_id_,
            [this](const std::shared_ptr<vsomeip::message> &msg) { on_event(msg); });

        app_->request_event(service_id_, instance_id_, event_id_,
                            {eventgroup_id_},
                            vsomeip::event_type_e::ET_EVENT);
        app_->subscribe(service_id_, instance_id_, eventgroup_id_);

        worker_ = std::thread([this]() { app_->start(); });
    }

    bool wait_for_service(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return available_.load(); });
    }

    void stop() override {
        if (app_) {
            app_->unsubscribe(service_id_, instance_id_, eventgroup_id_);
            app_->release_service(service_id_, instance_id_);
            app_->stop();
        }
        if (worker_.joinable()) worker_.join();
    }

private:
    void on_event(const std::shared_ptr<vsomeip::message> &msg) {
        auto pl = msg->get_payload();
        std::vector<vsomeip::byte_t> bytes(pl->get_data(), pl->get_data() + pl->get_length());
        VehicleEventPayload p = Serializer<VehicleEventPayload>::deserialize(bytes);
        std::cout << "[VEHICLE CLIENT] EVENT received: value=" << p.value << std::endl;
    }

    PatternRuntimeConfig cfg_;
    vsomeip::service_t service_id_ {};
    vsomeip::instance_t instance_id_ {};
    vsomeip::event_t event_id_ {};
    vsomeip::eventgroup_t eventgroup_id_ {};
    std::shared_ptr<vsomeip::application> app_;
    std::thread worker_;
    std::atomic<bool> available_ {false};
    std::mutex mutex_;
    std::condition_variable cv_;
};

int run_pattern_event(const PatternRuntimeConfig &cfg) {
    std::cout << "=== Pattern: Event (Publish/Subscribe) ===" << std::endl;
    std::cout << "Service=0x" << std::hex << cfg.service_id
              << " Instance=0x" << cfg.instance_id
              << " Event=0x" << cfg.event_id
              << " EventGroup=0x" << cfg.eventgroup_id << std::dec << std::endl;

    std::unique_ptr<EventService> svc;
    std::unique_ptr<EventClient> cli;

    if (cfg.role == RoleKind::Service || cfg.role == RoleKind::Both) {
        svc.reset(new EventService(cfg));
        svc->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    if (cfg.role == RoleKind::Client || cfg.role == RoleKind::Both) {
        cli.reset(new EventClient(cfg));
        cli->start();
        bool available = cli->wait_for_service(std::chrono::seconds(20));
        if (!available) {
            std::cerr << "[ERROR] Service unavailable (EVENT)." << std::endl;
            cli->stop();
            if (svc) svc->stop();
            return 1;
        }

        std::this_thread::sleep_for(std::chrono::seconds(cfg.client_run_seconds));
        cli->stop();
    }

    if (cfg.role == RoleKind::Service) {
        // Keep EVENT service alive long enough for an external client
        // in another namespace/process to discover and subscribe.
        std::this_thread::sleep_for(std::chrono::seconds(cfg.service_run_seconds));
    }

    if (svc) svc->stop();
    std::cout << "[OK] EVENT done." << std::endl;
    return 0;
}

}  // namespace dynamic_comm

