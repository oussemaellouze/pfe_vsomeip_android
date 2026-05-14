#pragma once

#include "core/DynamicCommLibrary.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

/// Requête : impulsions capteurs roue avant (ex. ABS / odométrie segment).
struct WheelTicksRequest {
    uint32_t front_left_ticks;
    uint32_t front_right_ticks;
};

/// Réponse : total d'impulsions fusionné sur l'intervalle (somme des deux voies).
struct VehicleOdometerResponse {
    uint32_t total_ticks;
};

namespace dynamic_comm {

template <>
struct Serializer<WheelTicksRequest> {
    static std::vector<vsomeip::byte_t> serialize(const WheelTicksRequest &request) {
        std::vector<vsomeip::byte_t> result;
        std::vector<vsomeip::byte_t> left = Serializer<uint32_t>::serialize(request.front_left_ticks);
        std::vector<vsomeip::byte_t> right = Serializer<uint32_t>::serialize(request.front_right_ticks);
        result.insert(result.end(), left.begin(), left.end());
        result.insert(result.end(), right.begin(), right.end());
        return result;
    }

    static WheelTicksRequest deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        if (bytes.size() != 8) {
            throw std::runtime_error("WheelTicksRequest payload must contain 8 bytes");
        }

        WheelTicksRequest request {};
        request.front_left_ticks = Serializer<uint32_t>::deserialize({bytes.begin(), bytes.begin() + 4});
        request.front_right_ticks = Serializer<uint32_t>::deserialize({bytes.begin() + 4, bytes.end()});
        return request;
    }
};

template <>
struct Serializer<VehicleOdometerResponse> {
    static std::vector<vsomeip::byte_t> serialize(const VehicleOdometerResponse &response) {
        return Serializer<uint32_t>::serialize(response.total_ticks);
    }

    static VehicleOdometerResponse deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        VehicleOdometerResponse response {};
        response.total_ticks = Serializer<uint32_t>::deserialize(bytes);
        return response;
    }
};

}  // namespace dynamic_comm
