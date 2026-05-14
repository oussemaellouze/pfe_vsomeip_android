#pragma once

#include "types/vehicle_action_types.hpp"

#include <iostream>
#include <memory>

/// Service Fire & Forget côté ECU : applique une commande (simulation par log).
class VehicleActionHandler : public dynamic_comm::IFireAndForgetHandler<VehicleActionCommand> {
public:
    void handle_oneway(const VehicleActionCommand &cmd) override {
        std::cout << "[ECU ACTION] Fire&Forget received — action_id=" << cmd.action_id
                  << " value=" << cmd.value << std::endl;
    }
};

