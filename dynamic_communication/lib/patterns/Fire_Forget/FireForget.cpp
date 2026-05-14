#include "patterns/Fire_Forget/FireForget.h"

#include "core/DynamicCommLibrary.h"

#include "modules/vehicle_action_modules.hpp"

#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

namespace dynamic_comm {

int run_pattern_fire_and_forget(const PatternRuntimeConfig &cfg) {
    std::cout << "=== Pattern: Fire & Forget (FF) ===" << std::endl;
    std::cout << "Service=0x" << std::hex << cfg.service_id
              << " Instance=0x" << cfg.instance_id
              << " Method=0x" << cfg.method_id << std::dec << std::endl;

    auto handler = std::make_shared<VehicleActionHandler>();

    std::unique_ptr<ServiceEndpoint> service_ep;
    std::unique_ptr<DynamicFireAndForgetClient<VehicleActionCommand>> client_ff;

    if (cfg.role == RoleKind::Service || cfg.role == RoleKind::Both) {
        service_ep.reset(new DynamicFireAndForgetService<VehicleActionCommand>(
            cfg.app_service_name,
            static_cast<vsomeip::service_t>(cfg.service_id),
            static_cast<vsomeip::instance_t>(cfg.instance_id),
            static_cast<vsomeip::method_t>(cfg.method_id),
            handler));
        service_ep->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    if (cfg.role == RoleKind::Client || cfg.role == RoleKind::Both) {
        client_ff.reset(new DynamicFireAndForgetClient<VehicleActionCommand>(
            cfg.app_client_name,
            static_cast<vsomeip::service_t>(cfg.service_id),
            static_cast<vsomeip::instance_t>(cfg.instance_id),
            static_cast<vsomeip::method_t>(cfg.method_id)));
        client_ff->start();

        bool available = client_ff->wait_for_service(std::chrono::seconds(20));
        if (!available) {
            std::cerr << "[ERROR] Service unavailable (FF)." << std::endl;
            client_ff->stop();
            if (service_ep) service_ep->stop();
            return 1;
        }

        std::cout << "[VEHICLE CLIENT] Sending FF: action_id=" << cfg.v1 << " value=" << cfg.v2 << std::endl;
        client_ff->send_fire_and_forget(VehicleActionCommand {cfg.v1, cfg.v2});
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
        client_ff->stop();
    }

    if (cfg.role == RoleKind::Service) {
        // Keep service alive long enough for an external client
        // (separate process/namespace) to discover and send requests.
        std::this_thread::sleep_for(std::chrono::seconds(cfg.service_run_seconds));
    }

    if (service_ep) service_ep->stop();
    std::cout << "[OK] FF done." << std::endl;
    return 0;
}

}  // namespace dynamic_comm

