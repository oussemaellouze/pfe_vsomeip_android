#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace dynamic_comm {

enum class PatternKind { RequestResponse, FireAndForget, Event, Field };
enum class RoleKind { Service, Client, Both };
enum class RRFusionKind { Sum, Average };
enum class RRUseCaseKind { BasicTicks, ComplexList, ComplexSet, ComplexMap };

struct PatternRuntimeConfig {
    PatternKind pattern = PatternKind::RequestResponse;
    RoleKind role = RoleKind::Both;
    RRFusionKind rr_fusion = RRFusionKind::Average;
    RRUseCaseKind rr_use_case = RRUseCaseKind::BasicTicks;

    uint32_t service_id = 0x3333;
    uint32_t instance_id = 0x0001;

    uint32_t method_id = 0x0100;

    uint32_t event_id = 0x8001;
    uint32_t eventgroup_id = 0x0001;

    uint32_t v1 = 21;
    uint32_t v2 = 21;

    std::string app_service_name = "dyn_service_app";
    std::string app_client_name = "dyn_client_app";

    bool wireshark_capture_enabled = false;
    std::string wireshark_capture_output_dir = "wireshark";
    std::string wireshark_capture_filename_prefix = "capture";
    std::string wireshark_capture_interface = "lo";
    std::string wireshark_capture_filter = "udp port 30490 or udp port 30509";
    uint32_t wireshark_capture_duration_seconds = 15;

    uint32_t max_iterations = 0;
    uint32_t service_run_seconds = 20;
    uint32_t client_run_seconds = 2;
};

inline uint32_t parse_u32_auto_base(const std::string &s) {
    std::size_t idx = 0;
    unsigned long v = std::stoul(s, &idx, 0);
    if (idx != s.size()) throw std::runtime_error("Invalid integer: " + s);
    if (v > 0xFFFFFFFFul) throw std::runtime_error("Out of range uint32: " + s);
    return static_cast<uint32_t>(v);
}

PatternRuntimeConfig load_runtime_config_from_json_file(const std::string &path);

/// Load from a unified config file: \p path must contain top-level \c "patterns" with an object
/// for \p pattern_key (e.g. \c rr, \c ff, \c rr_complex_list). That object may include \c pattern,
/// \c rr_fusion, \c rr_use_case, plus nested \c "service" and \c "client" objects with flat keys.
PatternRuntimeConfig load_runtime_config_from_unified_file(const std::string &path,
                                                           const std::string &pattern_key,
                                                           const std::string &role_str);

}  // namespace dynamic_comm

