#include "core/PatternRuntimeConfig.h"

#include <fstream>
#include <sstream>

namespace dynamic_comm {

static std::string read_all(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("Cannot open config file: " + path);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static std::string to_lower(std::string s) {
    for (char &c : s) {
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    }
    return s;
}

static std::string extract_string_value(const std::string &json, const std::string &key) {
    const std::string needle = "\"" + key + "\"";
    std::size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    pos = json.find('"', pos);
    if (pos == std::string::npos) return "";
    std::size_t end = json.find('"', pos + 1);
    if (end == std::string::npos) return "";
    return json.substr(pos + 1, end - (pos + 1));
}

static std::string extract_number_token(const std::string &json, const std::string &key) {
    const std::string needle = "\"" + key + "\"";
    std::size_t pos = json.find(needle);
    if (pos == std::string::npos) return "";
    pos = json.find(':', pos);
    if (pos == std::string::npos) return "";
    while (pos < json.size() && (json[pos] == ':' || json[pos] == ' ' || json[pos] == '\t' ||
                                 json[pos] == '\r' || json[pos] == '\n'))
        pos++;
    if (pos >= json.size()) return "";
    if (json[pos] == '"') {
        std::size_t end = json.find('"', pos + 1);
        if (end == std::string::npos) return "";
        return json.substr(pos + 1, end - (pos + 1));
    }
    std::size_t end = pos;
    while (end < json.size()) {
        char c = json[end];
        if (c == ',' || c == '}' || c == ' ' || c == '\t' || c == '\r' || c == '\n') break;
        end++;
    }
    return json.substr(pos, end - pos);
}

static PatternKind parse_pattern(const std::string &s_raw) {
    std::string s = to_lower(s_raw);
    if (s == "request_response" || s == "requestresponse" || s == "rr") return PatternKind::RequestResponse;
    if (s == "fire_and_forget" || s == "fireandforget" || s == "ff") return PatternKind::FireAndForget;
    if (s == "event" || s == "events" || s == "pubsub") return PatternKind::Event;
    if (s == "field" || s == "fields") return PatternKind::Field;
    throw std::runtime_error("Unknown pattern: " + s_raw);
}

static RoleKind parse_role(const std::string &s_raw) {
    std::string s = to_lower(s_raw);
    if (s == "service" || s == "server") return RoleKind::Service;
    if (s == "client") return RoleKind::Client;
    if (s == "both" || s == "combined") return RoleKind::Both;
    throw std::runtime_error("Unknown role: " + s_raw);
}

static RRFusionKind parse_rr_fusion(const std::string &s_raw) {
    std::string s = to_lower(s_raw);
    if (s == "sum" || s == "add" || s == "total") return RRFusionKind::Sum;
    if (s == "avg" || s == "average" || s == "mean" || s == "sum_div_2" || s == "sum/2") return RRFusionKind::Average;
    throw std::runtime_error("Unknown rr_fusion: " + s_raw);
}

static RRUseCaseKind parse_rr_use_case(const std::string &s_raw) {
    std::string s = to_lower(s_raw);
    if (s == "basic" || s == "ticks" || s == "basic_ticks") return RRUseCaseKind::BasicTicks;
    if (s == "list" || s == "complex_list") return RRUseCaseKind::ComplexList;
    if (s == "set" || s == "complex_set") return RRUseCaseKind::ComplexSet;
    if (s == "map" || s == "dict" || s == "dictionary" || s == "complex_map") return RRUseCaseKind::ComplexMap;
    throw std::runtime_error("Unknown rr_use_case: " + s_raw);
}

static std::size_t skip_ws(std::size_t i, const std::string &json) {
    while (i < json.size() && (json[i] == ' ' || json[i] == '\t' || json[i] == '\r' || json[i] == '\n'))
        i++;
    return i;
}

static std::string extract_braced_object(const std::string &json, std::size_t open_brace) {
    if (open_brace >= json.size() || json[open_brace] != '{')
        throw std::runtime_error("extract_braced_object: expected '{'");
    int depth = 0;
    for (std::size_t i = open_brace; i < json.size(); ++i) {
        if (json[i] == '{')
            depth++;
        else if (json[i] == '}') {
            depth--;
            if (depth == 0) return json.substr(open_brace, i - open_brace + 1);
        }
    }
    throw std::runtime_error("extract_braced_object: unbalanced braces");
}

/// Find `"key"` then ':' then the JSON object value; return that object including braces.
static std::string extract_object_for_key(const std::string &json, const std::string &key) {
    const std::string needle = "\"" + key + "\"";
    std::size_t p = json.find(needle);
    if (p == std::string::npos)
        throw std::runtime_error("Missing JSON key \"" + key + "\"");
    p = json.find(':', p + needle.size());
    if (p == std::string::npos) throw std::runtime_error("Missing ':' after key \"" + key + "\"");
    p = skip_ws(p + 1, json);
    if (p >= json.size() || json[p] != '{')
        throw std::runtime_error("Key \"" + key + "\" does not map to a JSON object");
    return extract_braced_object(json, p);
}

static bool parse_bool_value(const std::string &s_raw) {
    std::string s = to_lower(s_raw);
    if (s == "1" || s == "true" || s == "yes" || s == "on") return true;
    if (s == "0" || s == "false" || s == "no" || s == "off") return false;
    throw std::runtime_error("Unknown boolean value: " + s_raw);
}

static void apply_flat_json_to_config(PatternRuntimeConfig &cfg, const std::string &json) {
    std::string pattern = extract_string_value(json, "pattern");
    if (!pattern.empty()) cfg.pattern = parse_pattern(pattern);

    std::string role = extract_string_value(json, "role");
    if (!role.empty()) cfg.role = parse_role(role);

    std::string rr_fusion = extract_string_value(json, "rr_fusion");
    if (!rr_fusion.empty()) cfg.rr_fusion = parse_rr_fusion(rr_fusion);

    std::string rr_use_case = extract_string_value(json, "rr_use_case");
    if (!rr_use_case.empty()) cfg.rr_use_case = parse_rr_use_case(rr_use_case);

    std::string tok;

    tok = extract_number_token(json, "service_id");
    if (!tok.empty()) cfg.service_id = parse_u32_auto_base(tok);

    tok = extract_number_token(json, "instance_id");
    if (!tok.empty()) cfg.instance_id = parse_u32_auto_base(tok);

    tok = extract_number_token(json, "method_id");
    if (!tok.empty()) cfg.method_id = parse_u32_auto_base(tok);

    tok = extract_number_token(json, "event_id");
    if (!tok.empty()) cfg.event_id = parse_u32_auto_base(tok);

    tok = extract_number_token(json, "eventgroup_id");
    if (!tok.empty()) cfg.eventgroup_id = parse_u32_auto_base(tok);

    tok = extract_number_token(json, "v1");
    if (!tok.empty()) cfg.v1 = parse_u32_auto_base(tok);

    tok = extract_number_token(json, "v2");
    if (!tok.empty()) cfg.v2 = parse_u32_auto_base(tok);

    std::string asn = extract_string_value(json, "app_service_name");
    if (!asn.empty()) cfg.app_service_name = asn;
    std::string acn = extract_string_value(json, "app_client_name");
    if (!acn.empty()) cfg.app_client_name = acn;

    std::string capture_enabled = extract_string_value(json, "wireshark_capture_enabled");
    if (capture_enabled.empty()) capture_enabled = extract_number_token(json, "wireshark_capture_enabled");
    if (!capture_enabled.empty()) cfg.wireshark_capture_enabled = parse_bool_value(capture_enabled);

    std::string capture_dir = extract_string_value(json, "wireshark_capture_output_dir");
    if (!capture_dir.empty()) cfg.wireshark_capture_output_dir = capture_dir;

    std::string capture_prefix = extract_string_value(json, "wireshark_capture_filename_prefix");
    if (!capture_prefix.empty()) cfg.wireshark_capture_filename_prefix = capture_prefix;

    std::string capture_interface = extract_string_value(json, "wireshark_capture_interface");
    if (!capture_interface.empty()) cfg.wireshark_capture_interface = capture_interface;

    std::string capture_filter = extract_string_value(json, "wireshark_capture_filter");
    if (!capture_filter.empty()) cfg.wireshark_capture_filter = capture_filter;

    tok = extract_number_token(json, "wireshark_capture_duration_seconds");
    if (!tok.empty()) cfg.wireshark_capture_duration_seconds = parse_u32_auto_base(tok);

    tok = extract_number_token(json, "max_iterations");
    if (!tok.empty()) cfg.max_iterations = parse_u32_auto_base(tok);

    tok = extract_number_token(json, "service_run_seconds");
    if (!tok.empty()) cfg.service_run_seconds = parse_u32_auto_base(tok);

    tok = extract_number_token(json, "client_run_seconds");
    if (!tok.empty()) cfg.client_run_seconds = parse_u32_auto_base(tok);
}

/// Only keys that may live on the pattern block (not inside service/client) to avoid picking nested values.
static void apply_pattern_parent_fields(PatternRuntimeConfig &cfg, const std::string &pattern_obj) {
    std::string pattern = extract_string_value(pattern_obj, "pattern");
    if (!pattern.empty()) cfg.pattern = parse_pattern(pattern);

    std::string rr_fusion = extract_string_value(pattern_obj, "rr_fusion");
    if (!rr_fusion.empty()) cfg.rr_fusion = parse_rr_fusion(rr_fusion);

    std::string rr_use_case = extract_string_value(pattern_obj, "rr_use_case");
    if (!rr_use_case.empty()) cfg.rr_use_case = parse_rr_use_case(rr_use_case);
}

PatternRuntimeConfig load_runtime_config_from_json_file(const std::string &path) {
    std::string json = read_all(path);
    PatternRuntimeConfig cfg;
    apply_flat_json_to_config(cfg, json);
    return cfg;
}

PatternRuntimeConfig load_runtime_config_from_unified_file(const std::string &path,
                                                           const std::string &pattern_key,
                                                           const std::string &role_str) {
    std::string file = read_all(path);
    std::string patterns_block = extract_object_for_key(file, "patterns");
    std::string pattern_obj = extract_object_for_key(patterns_block, pattern_key);
    RoleKind role = parse_role(role_str);
    PatternRuntimeConfig cfg;
    apply_pattern_parent_fields(cfg, pattern_obj);
    apply_flat_json_to_config(cfg, pattern_obj);

    if (role == RoleKind::Both) {
        std::string service_obj = extract_object_for_key(pattern_obj, "service");
        std::string client_obj = extract_object_for_key(pattern_obj, "client");
        apply_flat_json_to_config(cfg, service_obj);
        apply_flat_json_to_config(cfg, client_obj);
        cfg.role = RoleKind::Both;
        return cfg;
    }

    std::string role_name = (role == RoleKind::Service) ? "service" : "client";
    std::string role_obj = extract_object_for_key(pattern_obj, role_name);

    apply_flat_json_to_config(cfg, role_obj);
    cfg.role = role;
    return cfg;
}

}  // namespace dynamic_comm

