#include "core/PatternRuntimeConfig.h"

#include "patterns/Event/Event.h"
#include "patterns/Fire_Forget/FireForget.h"
#include "patterns/Field/Field.h"
#include "patterns/Request_Response/RequestResponse.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <string>

static void print_usage(const char *exe) {
    printf("Usage:\n");
    printf("  %s <flat_config.json>\n", exe);
    printf("      Single JSON with pattern, role, IDs.\n\n");
    printf("  %s <unified.json> <pattern_key> <service|client>\n", exe);
    printf("      unified.json with top-level 'patterns' object.\n");
    printf("      pattern_key: rr, ff, event, field, rr_complex_list, etc.\n\n");
    printf("Supported keys (flat config):\n");
    printf("  pattern: request_response | rr | fire_and_forget | ff | event | field\n");
    printf("  role: service | client\n");
    printf("  rr_use_case: basic_ticks | complex_list | complex_set | complex_map\n");
    printf("  rr_fusion: sum | avg\n");
    printf("  service_id, instance_id, method_id, event_id, eventgroup_id, v1, v2\n");
    printf("  app_service_name, app_client_name\n");
}

static bool file_contains_patterns(const std::string &content) {
    return content.find("\"patterns\"") != std::string::npos;
}

static std::string extract_first_pattern_key(const std::string &content) {
    std::size_t pos = content.find("\"patterns\"");
    if (pos == std::string::npos) return "";
    pos = content.find('{', pos);
    if (pos == std::string::npos) return "";
    pos = content.find('"', pos + 1);
    if (pos == std::string::npos) return "";
    std::size_t end = content.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return content.substr(pos + 1, end - pos - 1);
}

static int run_config(const dynamic_comm::PatternRuntimeConfig &cfg) {
    int rc;
    switch (cfg.pattern) {
        case dynamic_comm::PatternKind::RequestResponse:
            rc = dynamic_comm::run_pattern_request_response(cfg);
            break;
        case dynamic_comm::PatternKind::FireAndForget:
            rc = dynamic_comm::run_pattern_fire_and_forget(cfg);
            break;
        case dynamic_comm::PatternKind::Event:
            rc = dynamic_comm::run_pattern_event(cfg);
            break;
        case dynamic_comm::PatternKind::Field:
            rc = dynamic_comm::run_pattern_field(cfg);
            break;
        default:
            fprintf(stderr, "[ERROR] Unsupported pattern.\n");
            rc = 1;
    }

    return rc;
}

static int run_one(const char *path) {
    dynamic_comm::PatternRuntimeConfig cfg = 
        dynamic_comm::load_runtime_config_from_json_file(path);
    return run_config(cfg);
}

int main(int argc, char **argv) {
    if (argc >= 2) {
        std::string arg = argv[1];
        if (arg == "-h" || arg == "--help") {
            print_usage(argv[0]);
            return 0;
        }

        // Unified mode: 3 arguments
        if (argc == 4) {
            dynamic_comm::PatternRuntimeConfig cfg =
                dynamic_comm::load_runtime_config_from_unified_file(argv[1], argv[2], argv[3]);
            switch (cfg.pattern) {
                case dynamic_comm::PatternKind::RequestResponse:
                    return dynamic_comm::run_pattern_request_response(cfg);
                case dynamic_comm::PatternKind::FireAndForget:
                    return dynamic_comm::run_pattern_fire_and_forget(cfg);
                case dynamic_comm::PatternKind::Event:
                    return dynamic_comm::run_pattern_event(cfg);
                case dynamic_comm::PatternKind::Field:
                    return dynamic_comm::run_pattern_field(cfg);
                default:
                    fprintf(stderr, "[ERROR] Unsupported pattern.\n");
                    return 1;
            }
        }

        // Flat mode: 2 arguments
        if (argc == 2) {
            std::ifstream in(argv[1]);
            if (in) {
                std::string content((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                if (file_contains_patterns(content)) {
                    std::string pattern_key = extract_first_pattern_key(content);
                    if (!pattern_key.empty()) {
                        return run_config(dynamic_comm::load_runtime_config_from_unified_file(argv[1], pattern_key, "both"));
                    }
                }
            }
            return run_one(argv[1]);
        }

        fprintf(stderr, "[ERROR] Wrong number of arguments.\n");
        print_usage(argv[0]);
        return 2;
    }

    print_usage(argv[0]);
    return 2;
}

