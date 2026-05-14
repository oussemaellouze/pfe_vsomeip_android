#include "patterns/Field/Field.h"

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

class FieldService : public ServiceEndpoint {
public:
    explicit FieldService(const PatternRuntimeConfig &cfg) : cfg_(cfg) {}

    void start() override {
        app_ = vsomeip::runtime::get()->create_application(cfg_.app_service_name);
        if (!app_ || !app_->init()) throw std::runtime_error("Failed to init FieldService");

        service_id_ = static_cast<vsomeip::service_t>(cfg_.service_id);
        instance_id_ = static_cast<vsomeip::instance_t>(cfg_.instance_id);
        field_event_id_ = static_cast<vsomeip::event_t>(cfg_.event_id);
        field_group_id_ = static_cast<vsomeip::eventgroup_t>(cfg_.eventgroup_id);

        value_.store(cfg_.v1);

        app_->register_state_handler([this](vsomeip::state_type_e state) {
            if (state == vsomeip::state_type_e::ST_REGISTERED) {
                app_->offer_service(service_id_, instance_id_);
                app_->offer_event(service_id_, instance_id_, field_event_id_,
                                  {field_group_id_},
                                  vsomeip::event_type_e::ET_FIELD,
                                  std::chrono::milliseconds::zero(),
                                  false, true);
            }
        });

        running_ = true;
        worker_ = std::thread([this]() { app_->start(); });
        notifier_ = std::thread([this]() { notify_loop(); });
    }

    void stop() override {
        running_ = false;
        if (notifier_.joinable()) notifier_.join();
        if (app_) {
            app_->stop_offer_service(service_id_, instance_id_);
            app_->stop();
        }
        if (worker_.joinable()) worker_.join();
    }

private:
    void notify_loop() {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        uint32_t counter = 0;
        while (running_) {
            if (cfg_.max_iterations > 0 && counter >= cfg_.max_iterations) {
                break;
            }
            VehicleEventPayload p {value_.load()};
            auto bytes = Serializer<VehicleEventPayload>::serialize(p);
            auto payload = vsomeip::runtime::get()->create_payload();
            payload->set_data(bytes);
            app_->notify(service_id_, instance_id_, field_event_id_, payload);
            std::cout << "[ECU FIELD] Notifier field_event=0x" << std::hex << cfg_.event_id
                      << std::dec << " value=" << p.value << std::endl;
            value_.fetch_add(1);
            counter++;
            std::this_thread::sleep_for(std::chrono::milliseconds(400));
        }
    }

    PatternRuntimeConfig cfg_;
    vsomeip::service_t service_id_ {};
    vsomeip::instance_t instance_id_ {};
    vsomeip::event_t field_event_id_ {};
    vsomeip::eventgroup_t field_group_id_ {};
    std::shared_ptr<vsomeip::application> app_;
    std::thread worker_;
    std::thread notifier_;
    std::atomic<bool> running_ {false};
    std::atomic<uint32_t> value_ {0};
};

class FieldClient : public ClientEndpoint {
public:
    explicit FieldClient(const PatternRuntimeConfig &cfg) : cfg_(cfg) {}

    void start() override {
        app_ = vsomeip::runtime::get()->create_application(cfg_.app_client_name);
        if (!app_ || !app_->init()) throw std::runtime_error("Failed to init FieldClient");

        service_id_ = static_cast<vsomeip::service_t>(cfg_.service_id);
        instance_id_ = static_cast<vsomeip::instance_t>(cfg_.instance_id);
        field_event_id_ = static_cast<vsomeip::event_t>(cfg_.event_id);
        field_group_id_ = static_cast<vsomeip::eventgroup_t>(cfg_.eventgroup_id);

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

        app_->register_message_handler(service_id_, instance_id_, field_event_id_,
            [this](const std::shared_ptr<vsomeip::message> &msg) { on_field_update(msg); });

        app_->request_event(service_id_, instance_id_, field_event_id_,
                            {field_group_id_},
                            vsomeip::event_type_e::ET_FIELD);
        app_->subscribe(service_id_, instance_id_, field_group_id_);

        worker_ = std::thread([this]() { app_->start(); });
    }

    bool wait_for_service(std::chrono::milliseconds timeout) {
        std::unique_lock<std::mutex> lock(mutex_);
        return cv_.wait_for(lock, timeout, [this]() { return available_.load(); });
    }

    void stop() override {
        if (app_) {
            app_->unsubscribe(service_id_, instance_id_, field_group_id_);
            app_->release_service(service_id_, instance_id_);
            app_->stop();
        }
        if (worker_.joinable()) worker_.join();
    }

private:
    void on_field_update(const std::shared_ptr<vsomeip::message> &msg) {
        auto pl = msg->get_payload();
        std::vector<vsomeip::byte_t> bytes(pl->get_data(), pl->get_data() + pl->get_length());
        VehicleEventPayload p = Serializer<VehicleEventPayload>::deserialize(bytes);
        std::cout << "[VEHICLE CLIENT] FIELD update received: value=" << p.value << std::endl;
    }

    PatternRuntimeConfig cfg_;
    vsomeip::service_t service_id_ {};
    vsomeip::instance_t instance_id_ {};
    vsomeip::event_t field_event_id_ {};
    vsomeip::eventgroup_t field_group_id_ {};
    std::shared_ptr<vsomeip::application> app_;
    std::thread worker_;
    std::atomic<bool> available_ {false};
    std::mutex mutex_;
    std::condition_variable cv_;
};

int run_pattern_field(const PatternRuntimeConfig &cfg) {
    std::cout << "=== Pattern: Field (simplified) ===" << std::endl;
    std::cout << "Service=0x" << std::hex << cfg.service_id
              << " Instance=0x" << cfg.instance_id
              << " FieldEvent=0x" << cfg.event_id
              << " EventGroup=0x" << cfg.eventgroup_id << std::dec << std::endl;

    std::unique_ptr<FieldService> svc;
    std::unique_ptr<FieldClient> cli;

    if (cfg.role == RoleKind::Service || cfg.role == RoleKind::Both) {
        svc.reset(new FieldService(cfg));
        svc->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    if (cfg.role == RoleKind::Client || cfg.role == RoleKind::Both) {
        cli.reset(new FieldClient(cfg));
        cli->start();
        bool available = cli->wait_for_service(std::chrono::seconds(20));
        if (!available) {
            std::cerr << "[ERROR] Service unavailable (FIELD)." << std::endl;
            cli->stop();
            if (svc) svc->stop();
            return 1;
        }
        std::this_thread::sleep_for(std::chrono::seconds(cfg.client_run_seconds));
        cli->stop();
    }

    if (cfg.role == RoleKind::Service) {
        // Keep FIELD service alive long enough for an external client
        // in another namespace/process to discover and subscribe.
        std::this_thread::sleep_for(std::chrono::seconds(cfg.service_run_seconds));
    }

    if (svc) svc->stop();
    std::cout << "[OK] FIELD done." << std::endl;
    return 0;
}

}  // namespace dynamic_comm

