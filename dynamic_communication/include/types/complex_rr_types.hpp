#pragma once

#include "core/DynamicCommLibrary.h"

#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

struct ComplexListRequest {
    std::string vin;
    std::vector<uint32_t> wheel_samples;
    std::vector<std::string> tags;
};

struct ComplexListResponse {
    uint32_t sample_count;
    uint32_t sample_sum;
    std::string dominant_tag;
};

struct ComplexSetRequest {
    std::set<uint32_t> active_wheels;
    std::set<std::string> active_alerts;
};

struct ComplexSetResponse {
    uint32_t wheel_count;
    uint32_t alert_count;
    std::string highest_alert;
};

struct ComplexMapRequest {
    std::map<std::string, uint32_t> sensor_values;
};

struct ComplexMapResponse {
    uint32_t metric_count;
    uint32_t total_value;
    std::string max_metric_key;
};

namespace dynamic_comm {

template <>
struct Serializer<ComplexListRequest> {
    static std::vector<vsomeip::byte_t> serialize(const ComplexListRequest &v) {
        std::vector<vsomeip::byte_t> out;
        detail::append_with_size_prefix(out, Serializer<std::string>::serialize(v.vin));
        detail::append_with_size_prefix(out, Serializer<std::vector<uint32_t>>::serialize(v.wheel_samples));
        detail::append_with_size_prefix(out, Serializer<std::vector<std::string>>::serialize(v.tags));
        return out;
    }

    static ComplexListRequest deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        ComplexListRequest out {};
        std::size_t offset = 0;
        out.vin = Serializer<std::string>::deserialize(detail::read_sized_chunk(bytes, offset));
        out.wheel_samples = Serializer<std::vector<uint32_t>>::deserialize(detail::read_sized_chunk(bytes, offset));
        out.tags = Serializer<std::vector<std::string>>::deserialize(detail::read_sized_chunk(bytes, offset));
        if (offset != bytes.size()) {
            throw std::runtime_error("Malformed ComplexListRequest trailing bytes");
        }
        return out;
    }
};

template <>
struct Serializer<ComplexListResponse> {
    static std::vector<vsomeip::byte_t> serialize(const ComplexListResponse &v) {
        std::vector<vsomeip::byte_t> out;
        detail::append_with_size_prefix(out, Serializer<uint32_t>::serialize(v.sample_count));
        detail::append_with_size_prefix(out, Serializer<uint32_t>::serialize(v.sample_sum));
        detail::append_with_size_prefix(out, Serializer<std::string>::serialize(v.dominant_tag));
        return out;
    }

    static ComplexListResponse deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        ComplexListResponse out {};
        std::size_t offset = 0;
        out.sample_count = Serializer<uint32_t>::deserialize(detail::read_sized_chunk(bytes, offset));
        out.sample_sum = Serializer<uint32_t>::deserialize(detail::read_sized_chunk(bytes, offset));
        out.dominant_tag = Serializer<std::string>::deserialize(detail::read_sized_chunk(bytes, offset));
        if (offset != bytes.size()) {
            throw std::runtime_error("Malformed ComplexListResponse trailing bytes");
        }
        return out;
    }
};

template <>
struct Serializer<ComplexSetRequest> {
    static std::vector<vsomeip::byte_t> serialize(const ComplexSetRequest &v) {
        std::vector<vsomeip::byte_t> out;
        detail::append_with_size_prefix(out, Serializer<std::set<uint32_t>>::serialize(v.active_wheels));
        detail::append_with_size_prefix(out, Serializer<std::set<std::string>>::serialize(v.active_alerts));
        return out;
    }

    static ComplexSetRequest deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        ComplexSetRequest out {};
        std::size_t offset = 0;
        out.active_wheels = Serializer<std::set<uint32_t>>::deserialize(detail::read_sized_chunk(bytes, offset));
        out.active_alerts = Serializer<std::set<std::string>>::deserialize(detail::read_sized_chunk(bytes, offset));
        if (offset != bytes.size()) {
            throw std::runtime_error("Malformed ComplexSetRequest trailing bytes");
        }
        return out;
    }
};

template <>
struct Serializer<ComplexSetResponse> {
    static std::vector<vsomeip::byte_t> serialize(const ComplexSetResponse &v) {
        std::vector<vsomeip::byte_t> out;
        detail::append_with_size_prefix(out, Serializer<uint32_t>::serialize(v.wheel_count));
        detail::append_with_size_prefix(out, Serializer<uint32_t>::serialize(v.alert_count));
        detail::append_with_size_prefix(out, Serializer<std::string>::serialize(v.highest_alert));
        return out;
    }

    static ComplexSetResponse deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        ComplexSetResponse out {};
        std::size_t offset = 0;
        out.wheel_count = Serializer<uint32_t>::deserialize(detail::read_sized_chunk(bytes, offset));
        out.alert_count = Serializer<uint32_t>::deserialize(detail::read_sized_chunk(bytes, offset));
        out.highest_alert = Serializer<std::string>::deserialize(detail::read_sized_chunk(bytes, offset));
        if (offset != bytes.size()) {
            throw std::runtime_error("Malformed ComplexSetResponse trailing bytes");
        }
        return out;
    }
};

template <>
struct Serializer<ComplexMapRequest> {
    static std::vector<vsomeip::byte_t> serialize(const ComplexMapRequest &v) {
        return Serializer<std::map<std::string, uint32_t>>::serialize(v.sensor_values);
    }

    static ComplexMapRequest deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        ComplexMapRequest out {};
        out.sensor_values = Serializer<std::map<std::string, uint32_t>>::deserialize(bytes);
        return out;
    }
};

template <>
struct Serializer<ComplexMapResponse> {
    static std::vector<vsomeip::byte_t> serialize(const ComplexMapResponse &v) {
        std::vector<vsomeip::byte_t> out;
        detail::append_with_size_prefix(out, Serializer<uint32_t>::serialize(v.metric_count));
        detail::append_with_size_prefix(out, Serializer<uint32_t>::serialize(v.total_value));
        detail::append_with_size_prefix(out, Serializer<std::string>::serialize(v.max_metric_key));
        return out;
    }

    static ComplexMapResponse deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        ComplexMapResponse out {};
        std::size_t offset = 0;
        out.metric_count = Serializer<uint32_t>::deserialize(detail::read_sized_chunk(bytes, offset));
        out.total_value = Serializer<uint32_t>::deserialize(detail::read_sized_chunk(bytes, offset));
        out.max_metric_key = Serializer<std::string>::deserialize(detail::read_sized_chunk(bytes, offset));
        if (offset != bytes.size()) {
            throw std::runtime_error("Malformed ComplexMapResponse trailing bytes");
        }
        return out;
    }
};

}  // namespace dynamic_comm
