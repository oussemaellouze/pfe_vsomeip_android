#pragma once

#include "core/DynamicCommLibrary.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

/// Payload d'événement : compteur/valeur télémétrie simple.
struct VehicleEventPayload {
    uint32_t value;
};

namespace dynamic_comm {

template <>
struct Serializer<VehicleEventPayload> {
    static std::vector<vsomeip::byte_t> serialize(const VehicleEventPayload &p) {
        return Serializer<uint32_t>::serialize(p.value);
    }

    static VehicleEventPayload deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        VehicleEventPayload p {};
        p.value = Serializer<uint32_t>::deserialize(bytes);
        return p;
    }
};

}  // namespace dynamic_comm

