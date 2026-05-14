#pragma once

#include "core/DynamicCommLibrary.h"

#include <cstdint>
#include <stdexcept>
#include <vector>

/// Requête Fire & Forget : commande d'action véhicule (ex: verrouillage portes, clignotant...).
/// Interprétation applicative : action_id = type d'action ; value = paramètre (0/1, intensité, etc.).
struct VehicleActionCommand {
    uint32_t action_id;
    uint32_t value;
};

namespace dynamic_comm {

template <>
struct Serializer<VehicleActionCommand> {
    static std::vector<vsomeip::byte_t> serialize(const VehicleActionCommand &cmd) {
        std::vector<vsomeip::byte_t> result;
        std::vector<vsomeip::byte_t> a = Serializer<uint32_t>::serialize(cmd.action_id);
        std::vector<vsomeip::byte_t> v = Serializer<uint32_t>::serialize(cmd.value);
        result.insert(result.end(), a.begin(), a.end());
        result.insert(result.end(), v.begin(), v.end());
        return result;
    }

    static VehicleActionCommand deserialize(const std::vector<vsomeip::byte_t> &bytes) {
        if (bytes.size() != 8) {
            throw std::runtime_error("VehicleActionCommand payload must contain 8 bytes");
        }

        VehicleActionCommand cmd {};
        cmd.action_id = Serializer<uint32_t>::deserialize({bytes.begin(), bytes.begin() + 4});
        cmd.value = Serializer<uint32_t>::deserialize({bytes.begin() + 4, bytes.end()});
        return cmd;
    }
};

}  // namespace dynamic_comm

