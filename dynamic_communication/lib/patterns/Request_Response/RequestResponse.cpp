#include "patterns/Request_Response/RequestResponse.h"

#include "core/DynamicCommLibrary.h"

#include "modules/complex_vehicle_modules.hpp"
#include "modules/vehicle_modules.hpp"

#include <atomic>
#include <chrono>
#include <functional>
#include <iostream>
#include <memory>
#include <thread>

namespace dynamic_comm {

template <typename TRequest, typename TResponse>
static int run_rr_case(const PatternRuntimeConfig &cfg,
                       const std::string &case_label,
                       const std::shared_ptr<IRequestHandler<TRequest, TResponse>> &handler,
                       const std::shared_ptr<IResponseConsumer<TResponse>> &consumer,
                       const TRequest &request_payload,
                       std::atomic<bool> &done) {
    std::cout << "=== Pattern: Request/Response (RR) | use_case=" << case_label << " ===" << std::endl;
    std::cout << "Service=0x" << std::hex << cfg.service_id
              << " Instance=0x" << cfg.instance_id
              << " Method=0x" << cfg.method_id << std::dec << std::endl;

    std::unique_ptr<ServiceEndpoint> service_ep;
    std::unique_ptr<DynamicClient<TRequest, TResponse>> client_rr;

    if (cfg.role == RoleKind::Service || cfg.role == RoleKind::Both) {
        service_ep.reset(new DynamicService<TRequest, TResponse>(
            cfg.app_service_name,
            static_cast<vsomeip::service_t>(cfg.service_id),
            static_cast<vsomeip::instance_t>(cfg.instance_id),
            static_cast<vsomeip::method_t>(cfg.method_id),
            handler));
        service_ep->start();
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }

    if (cfg.role == RoleKind::Client || cfg.role == RoleKind::Both) {
        client_rr.reset(new DynamicClient<TRequest, TResponse>(
            cfg.app_client_name,
            static_cast<vsomeip::service_t>(cfg.service_id),
            static_cast<vsomeip::instance_t>(cfg.instance_id),
            static_cast<vsomeip::method_t>(cfg.method_id),
            consumer));
        client_rr->start();

        bool available = client_rr->wait_for_service(std::chrono::seconds(20));
        if (!available) {
            std::cerr << "[ERROR] Service unavailable (RR)." << std::endl;
            client_rr->stop();
            if (service_ep) service_ep->stop();
            return 1;
        }

        client_rr->send_request(request_payload);
        for (int i = 0; i < 30 && !done.load(); ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        client_rr->stop();
    }

    if (cfg.role == RoleKind::Service) {
        std::this_thread::sleep_for(std::chrono::seconds(20));
    }

    if (service_ep) service_ep->stop();

    if (cfg.role == RoleKind::Client || cfg.role == RoleKind::Both) {
        if (!done.load()) {
            std::cerr << "[ERROR] Response timeout (RR)." << std::endl;
            return 1;
        }
    }

    std::cout << "[OK] RR done." << std::endl;
    return 0;
}

int run_pattern_request_response(const PatternRuntimeConfig &cfg) {
    std::atomic<bool> done(false);

    if (cfg.rr_use_case == RRUseCaseKind::ComplexList) {
        auto handler = std::make_shared<ComplexListHandler>();
        auto consumer = std::make_shared<ComplexListConsumer>(done);
        ComplexListRequest request {
            "WVWZZZ1JZXW000001",
            {cfg.v1, cfg.v2, cfg.v1 + cfg.v2},
            {"urban", "eco", "wet"}};
        return run_rr_case<ComplexListRequest, ComplexListResponse>(cfg, "complex_list", handler, consumer, request, done);
    }

    if (cfg.rr_use_case == RRUseCaseKind::ComplexSet) {
        auto handler = std::make_shared<ComplexSetHandler>();
        auto consumer = std::make_shared<ComplexSetConsumer>(done);
        ComplexSetRequest request {
            {1U, 2U, (cfg.v1 % 4U) + 1U},
            {"ABS", "BRAKE", (cfg.v2 % 2U == 0U) ? "ESP" : "TCS"}};
        return run_rr_case<ComplexSetRequest, ComplexSetResponse>(cfg, "complex_set", handler, consumer, request, done);
    }

    if (cfg.rr_use_case == RRUseCaseKind::ComplexMap) {
        auto handler = std::make_shared<ComplexMapHandler>();
        auto consumer = std::make_shared<ComplexMapConsumer>(done);
        ComplexMapRequest request {
            {{"speed_kmh", cfg.v1}, {"rpm_x100", cfg.v2}, {"coolant_c", cfg.v1 + cfg.v2}}};
        return run_rr_case<ComplexMapRequest, ComplexMapResponse>(cfg, "complex_map", handler, consumer, request, done);
    }

    auto handler = std::make_shared<VehicleTelemetryHandler>(cfg.rr_fusion);
    auto consumer = std::make_shared<VehicleTelemetryConsumer>(done);
    return run_rr_case<WheelTicksRequest, VehicleOdometerResponse>(
        cfg,
        "basic_ticks",
        handler,
        consumer,
        WheelTicksRequest {cfg.v1, cfg.v2},
        done);
}

}  // namespace dynamic_comm

