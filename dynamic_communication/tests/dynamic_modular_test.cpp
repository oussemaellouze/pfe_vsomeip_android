#include "modules/complex_vehicle_modules.hpp"
#include "modules/vehicle_modules.hpp"
#include "types/complex_types.hpp"
#include "types/complex_rr_types.hpp"

#include <atomic>
#include <cstdint>
#include <iostream>
#include <map>
#include <set>
#include <vector>

int main() {
    bool ok = true;

    WheelTicksRequest req {10, 32};
    std::vector<vsomeip::byte_t> req_bytes = dynamic_comm::Serializer<WheelTicksRequest>::serialize(req);
    WheelTicksRequest req_roundtrip = dynamic_comm::Serializer<WheelTicksRequest>::deserialize(req_bytes);

    if (req_roundtrip.front_left_ticks != 10 || req_roundtrip.front_right_ticks != 32) {
        std::cerr << "[FAIL] WheelTicksRequest serializer roundtrip failed." << std::endl;
        ok = false;
    }

    VehicleTelemetryHandler handler(dynamic_comm::RRFusionKind::Sum);
    VehicleOdometerResponse resp = handler.handle_request(req_roundtrip);
    if (resp.total_ticks != 42) {
        std::cerr << "[FAIL] VehicleTelemetryHandler expected total_ticks 42, got " << resp.total_ticks << std::endl;
        ok = false;
    }

    std::vector<vsomeip::byte_t> resp_bytes = dynamic_comm::Serializer<VehicleOdometerResponse>::serialize(resp);
    VehicleOdometerResponse resp_roundtrip = dynamic_comm::Serializer<VehicleOdometerResponse>::deserialize(resp_bytes);
    if (resp_roundtrip.total_ticks != 42) {
        std::cerr << "[FAIL] VehicleOdometerResponse serializer roundtrip failed." << std::endl;
        ok = false;
    }

    std::atomic<bool> done(false);
    VehicleTelemetryConsumer consumer(done);
    consumer.on_response(resp_roundtrip, vsomeip::return_code_e::E_OK);

    if (!done.load() || consumer.last_total_ticks() != 42) {
        std::cerr << "[FAIL] VehicleTelemetryConsumer failed to capture response." << std::endl;
        ok = false;
    }

    VehicleDiagnosticSnapshot diag {
        "VF1ABCD1234567890",
        {0x1001u, 0x2002u, 0x3003u},
        {"powertrain", "sensor", "critical"}};
    std::vector<vsomeip::byte_t> diag_bytes = dynamic_comm::Serializer<VehicleDiagnosticSnapshot>::serialize(diag);
    VehicleDiagnosticSnapshot diag_roundtrip = dynamic_comm::Serializer<VehicleDiagnosticSnapshot>::deserialize(diag_bytes);
    if (diag_roundtrip.vin != diag.vin ||
        diag_roundtrip.dtc_codes != diag.dtc_codes ||
        diag_roundtrip.tags != diag.tags) {
        std::cerr << "[FAIL] VehicleDiagnosticSnapshot serializer roundtrip failed." << std::endl;
        ok = false;
    }

    ComplexListRequest list_req {
        "WVWZZZ1JZXW000001",
        {11U, 22U, 33U},
        {"urban", "eco"}};
    std::vector<vsomeip::byte_t> list_req_bytes = dynamic_comm::Serializer<ComplexListRequest>::serialize(list_req);
    ComplexListRequest list_req_roundtrip = dynamic_comm::Serializer<ComplexListRequest>::deserialize(list_req_bytes);
    if (list_req_roundtrip.vin != list_req.vin ||
        list_req_roundtrip.wheel_samples != list_req.wheel_samples ||
        list_req_roundtrip.tags != list_req.tags) {
        std::cerr << "[FAIL] ComplexListRequest serializer roundtrip failed." << std::endl;
        ok = false;
    }

    std::atomic<bool> list_done(false);
    ComplexListHandler list_handler;
    ComplexListResponse list_resp = list_handler.handle_request(list_req_roundtrip);
    ComplexListConsumer list_consumer(list_done);
    list_consumer.on_response(list_resp, vsomeip::return_code_e::E_OK);
    if (!list_done.load() || list_resp.sample_count != 3U || list_resp.sample_sum != 66U) {
        std::cerr << "[FAIL] ComplexList handler/consumer flow failed." << std::endl;
        ok = false;
    }

    ComplexSetRequest set_req {
        {1U, 2U, 4U},
        {"ABS", "ESP", "TCS"}};
    std::vector<vsomeip::byte_t> set_req_bytes = dynamic_comm::Serializer<ComplexSetRequest>::serialize(set_req);
    ComplexSetRequest set_req_roundtrip = dynamic_comm::Serializer<ComplexSetRequest>::deserialize(set_req_bytes);
    if (set_req_roundtrip.active_wheels != set_req.active_wheels ||
        set_req_roundtrip.active_alerts != set_req.active_alerts) {
        std::cerr << "[FAIL] ComplexSetRequest serializer roundtrip failed." << std::endl;
        ok = false;
    }

    ComplexMapRequest map_req {
        {{"speed_kmh", 100U}, {"rpm_x100", 31U}, {"coolant_c", 92U}}};
    std::vector<vsomeip::byte_t> map_req_bytes = dynamic_comm::Serializer<ComplexMapRequest>::serialize(map_req);
    ComplexMapRequest map_req_roundtrip = dynamic_comm::Serializer<ComplexMapRequest>::deserialize(map_req_bytes);
    if (map_req_roundtrip.sensor_values != map_req.sensor_values) {
        std::cerr << "[FAIL] ComplexMapRequest serializer roundtrip failed." << std::endl;
        ok = false;
    }

    if (!ok) {
        return 1;
    }

    std::cout << "[PASS] dynamic_modular_test passed." << std::endl;
    return 0;
}
