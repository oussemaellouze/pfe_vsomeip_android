#pragma once

#include "core/PatternRuntimeConfig.h"
#include "types/complex_rr_types.hpp"

#include <atomic>
#include <iostream>

class ComplexListHandler : public dynamic_comm::IRequestHandler<ComplexListRequest, ComplexListResponse> {
public:
    ComplexListResponse handle_request(const ComplexListRequest &request) override {
        ComplexListResponse response {};
        response.sample_count = static_cast<uint32_t>(request.wheel_samples.size());
        uint32_t sum = 0;
        for (uint32_t sample : request.wheel_samples) {
            sum += sample;
        }
        response.sample_sum = sum;
        response.dominant_tag = request.tags.empty() ? "none" : request.tags.front();

        std::cout << "[ECU SERVICE] Complex LIST request vin=" << request.vin
                  << " samples=" << response.sample_count
                  << " sum=" << response.sample_sum
                  << " dominant_tag=" << response.dominant_tag << std::endl;
        return response;
    }
};

class ComplexListConsumer : public dynamic_comm::IResponseConsumer<ComplexListResponse> {
public:
    explicit ComplexListConsumer(std::atomic<bool> &done_ref) : done_(done_ref) {}

    void on_response(const ComplexListResponse &response, vsomeip::return_code_e code) override {
        std::cout << "[VEHICLE CLIENT] Return code: 0x" << std::hex << static_cast<int>(code)
                  << std::dec << " | LIST response count=" << response.sample_count
                  << " sum=" << response.sample_sum
                  << " dominant_tag=" << response.dominant_tag << std::endl;
        done_ = true;
    }

private:
    std::atomic<bool> &done_;
};

class ComplexSetHandler : public dynamic_comm::IRequestHandler<ComplexSetRequest, ComplexSetResponse> {
public:
    ComplexSetResponse handle_request(const ComplexSetRequest &request) override {
        ComplexSetResponse response {};
        response.wheel_count = static_cast<uint32_t>(request.active_wheels.size());
        response.alert_count = static_cast<uint32_t>(request.active_alerts.size());
        response.highest_alert = request.active_alerts.empty() ? "none" : *request.active_alerts.rbegin();

        std::cout << "[ECU SERVICE] Complex SET request wheels=" << response.wheel_count
                  << " alerts=" << response.alert_count
                  << " highest_alert=" << response.highest_alert << std::endl;
        return response;
    }
};

class ComplexSetConsumer : public dynamic_comm::IResponseConsumer<ComplexSetResponse> {
public:
    explicit ComplexSetConsumer(std::atomic<bool> &done_ref) : done_(done_ref) {}

    void on_response(const ComplexSetResponse &response, vsomeip::return_code_e code) override {
        std::cout << "[VEHICLE CLIENT] Return code: 0x" << std::hex << static_cast<int>(code)
                  << std::dec << " | SET response wheels=" << response.wheel_count
                  << " alerts=" << response.alert_count
                  << " highest_alert=" << response.highest_alert << std::endl;
        done_ = true;
    }

private:
    std::atomic<bool> &done_;
};

class ComplexMapHandler : public dynamic_comm::IRequestHandler<ComplexMapRequest, ComplexMapResponse> {
public:
    ComplexMapResponse handle_request(const ComplexMapRequest &request) override {
        ComplexMapResponse response {};
        response.metric_count = static_cast<uint32_t>(request.sensor_values.size());
        response.total_value = 0;
        response.max_metric_key = "none";
        uint32_t max_value = 0;

        for (const auto &entry : request.sensor_values) {
            response.total_value += entry.second;
            if (response.max_metric_key == "none" || entry.second > max_value) {
                max_value = entry.second;
                response.max_metric_key = entry.first;
            }
        }

        std::cout << "[ECU SERVICE] Complex MAP request metrics=" << response.metric_count
                  << " total=" << response.total_value
                  << " max_metric_key=" << response.max_metric_key << std::endl;
        return response;
    }
};

class ComplexMapConsumer : public dynamic_comm::IResponseConsumer<ComplexMapResponse> {
public:
    explicit ComplexMapConsumer(std::atomic<bool> &done_ref) : done_(done_ref) {}

    void on_response(const ComplexMapResponse &response, vsomeip::return_code_e code) override {
        std::cout << "[VEHICLE CLIENT] Return code: 0x" << std::hex << static_cast<int>(code)
                  << std::dec << " | MAP response metrics=" << response.metric_count
                  << " total=" << response.total_value
                  << " max_metric_key=" << response.max_metric_key << std::endl;
        done_ = true;
    }

private:
    std::atomic<bool> &done_;
};
