#pragma once

#include "core/DynamicCommLibrary.h"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

// Example of a complex payload containing strings and array lists.
struct VehicleDiagnosticSnapshot {
    std::string vin;
    std::vector<uint32_t> dtc_codes;
    std::vector<std::string> tags;
};

namespace dynamic_comm {

template <>
struct Serializer<VehicleDiagnosticSnapshot> {
    static std::vector<vsomeip::byte_t> serialize(const VehicleDiagnosticSnapshot &v) {
        std::vector<vsomeip::byte_t> out;
        std::vector<vsomeip::byte_t> vin_bytes = Serializer<std::string>::serialize(v.vin);
        std::vector<vsomeip::byte_t> dtc_bytes = Serializer<std::vector<uint32_t>>::serialize(v.dtc_codes);
        std::vector<vsomeip::byte_t> tags_bytes = Serializer<std::vector<std::string>>::serialize(v.tags);

        detail::append_with_size_prefix(out, vin_bytes);
        detail::append_with_size_prefix(out, dtc_bytes);
        detail::append_with_size_prefix(out, tags_bytes);
        return out;
    }

    static VehicleDiagnosticSnapshot deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        VehicleDiagnosticSnapshot snapshot {};
        std::size_t offset = 0;

        std::vector<vsomeip::byte_t> vin_chunk = detail::read_sized_chunk(bytes, offset);
        snapshot.vin = Serializer<std::string>::deserialize(vin_chunk);

        std::vector<vsomeip::byte_t> dtc_chunk = detail::read_sized_chunk(bytes, offset);
        snapshot.dtc_codes = Serializer<std::vector<uint32_t>>::deserialize(dtc_chunk);

        std::vector<vsomeip::byte_t> tags_chunk = detail::read_sized_chunk(bytes, offset);
        snapshot.tags = Serializer<std::vector<std::string>>::deserialize(tags_chunk);

        if (offset != bytes.size()) {
            throw std::runtime_error("Malformed VehicleDiagnosticSnapshot trailing bytes");
        }
        return snapshot;
    }
};

}  // namespace dynamic_comm

