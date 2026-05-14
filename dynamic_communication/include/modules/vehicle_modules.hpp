#pragma once

#include "core/PatternRuntimeConfig.h"
#include "types/vehicle_types.hpp"

#include <atomic>
#include <iostream>

/// Service embarqué : agrège les impulsions roues avant (fusion odométrique simple).
class VehicleTelemetryHandler : public dynamic_comm::IRequestHandler<WheelTicksRequest, VehicleOdometerResponse> {
public:
    explicit VehicleTelemetryHandler(dynamic_comm::RRFusionKind fusion) : fusion_(fusion) {}

    VehicleOdometerResponse handle_request(const WheelTicksRequest &request) override {
        VehicleOdometerResponse response {};
        const uint32_t sum = request.front_left_ticks + request.front_right_ticks;
        if (fusion_ == dynamic_comm::RRFusionKind::Sum) {
            response.total_ticks = sum;
            std::cout << "[ECU SERVICE] Vehicle odometry — fuse by sum: front-left "
                      << request.front_left_ticks << " + front-right " << request.front_right_ticks
                      << " => fused segment ticks = " << response.total_ticks << std::endl;
        } else {
            response.total_ticks = sum / 2U;
            std::cout << "[ECU SERVICE] Vehicle odometry — fuse by average: (front-left "
                      << request.front_left_ticks << " + front-right " << request.front_right_ticks
                      << ") / 2 => fused segment ticks = " << response.total_ticks << std::endl;
        }
        return response;
    }

private:
    dynamic_comm::RRFusionKind fusion_;
};

/// Client embarqué (ex. calculateur affichage / enregistreur) : reçoit le total fusionné.
class VehicleTelemetryConsumer : public dynamic_comm::IResponseConsumer<VehicleOdometerResponse> {
public:
    explicit VehicleTelemetryConsumer(std::atomic<bool> &done_ref) : done_(done_ref), last_total_ticks_(0) {}

    void on_response(const VehicleOdometerResponse &response, vsomeip::return_code_e code) override {
        std::cout << "[VEHICLE CLIENT] Return code: 0x" << std::hex << static_cast<int>(code)
                  << std::dec << " | fused segment ticks: " << response.total_ticks << std::endl;
        last_total_ticks_ = response.total_ticks;
        done_ = true;
    }

    uint32_t last_total_ticks() const { return last_total_ticks_; }

private:
    std::atomic<bool> &done_;
    uint32_t last_total_ticks_;
};
