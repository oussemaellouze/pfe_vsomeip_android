# PFE vSOME/IP Tests - Usage Guide

## Quick Start

### Run Service/Client with Automatic Capture

```bash
bash dynamic_communication/run_wireshark_test.sh
```

**What it does:**
1. ✓ Launches SOME/IP service (listens on port 30509/UDP)
2. ✓ Launches SOME/IP client (sends WheelTicksRequest)
3. ✓ Service processes request and returns fused response
4. ⚠️ Attempts network capture (requires elevated privileges)

**Output example:**
```
[ECU SERVICE] Vehicle odometry — fuse by sum: front-left 100 + front-right 200 => fused segment ticks = 300
[VEHICLE CLIENT] Return code: 0x0 | fused segment ticks: 300
[OK] RR done.
```

---

## Configuration Files

### Minimal Configuration Set

```
config/
├── vsomeip_dynamic_combined.json    ← SOME/IP stack configuration (ports, IPs, apps)
├── rr_service.json                  ← Service configuration (flat JSON format)
├── rr_client.json                   ← Client configuration (flat JSON format)
└── README.md                         ← Configuration details
```

### Alternative: Unified Config Format

`config/wireshark.json` — Nested format with service + client combined
- Not used by default scripts
- Useful for: `./build/main config/wireshark.json rr service`

---

## Project Structure (Cleaned)

```
dynamic_communication/
├── build/                      ← Compiled executables
│   └── main                    ← Main SOME/IP application
├── config/                     ← JSON configuration files (minimal)
├── include/
│   ├── modules/                ← Pattern module interfaces
│   └── types/                  ← Data structures (vehicle_types.hpp)
├── lib/
│   ├── core/                   ← PatternRuntimeConfig parser
│   └── patterns/               ← RR, FF, Event, Field implementations
├── src/
│   └── main.cpp                ← Entry point (C++14 compatible)
├── tests/                       ← Unit tests
├── wireshark/                  ← Network capture storage (auto-created)
├── run_wireshark_test.sh       ← Automation script
├── CMakeLists.txt
└── USAGE.md                     ← This file
```

**Removed:**
- `ui/` — Web interface (not needed for JSON-based testing)
- `docs/` — Archived documentation

---

## Running Individual Components

### Service Only
```bash
cd dynamic_communication/build
export VSOMEIP_CONFIGURATION="$(pwd)/../config/vsomeip_dynamic_combined.json"
export LD_LIBRARY_PATH="$(pwd)/../../_install/lib:$LD_LIBRARY_PATH"
./main ../config/rr_service.json &
```

### Client Only (connects to running service)
```bash
cd dynamic_communication/build
export VSOMEIP_CONFIGURATION="$(pwd)/../config/vsomeip_dynamic_combined.json"
export LD_LIBRARY_PATH="$(pwd)/../../_install/lib:$LD_LIBRARY_PATH"
./main ../config/rr_client.json
```

### Using Unified Config
```bash
./build/main config/wireshark.json rr service     # Service mode
./build/main config/wireshark.json rr client      # Client mode
```

---

## Network Capture

### Automatic Capture (via run_wireshark_test.sh)

The script attempts to capture network traffic to `wireshark/capture_YYYYMMDD_HHMMSS.pcapng`

**Permission Options:**

1. **Set capabilities (recommended, no password needed):**
   ```bash
   sudo setcap cap_net_raw,cap_net_admin=eip /usr/bin/tcpdump
   ```
   After this, `run_wireshark_test.sh` will capture without sudo prompt.

2. **Add user to tcpdump group:**
   ```bash
   sudo usermod -a -G tcpdump $USER
   newgrp tcpdump
   ```

3. **Use Wireshark GUI (requires sudo once):**
   ```bash
   sudo wireshark &
   # Start capture on lo interface with filter:
   # udp.port == 30490 or udp.port == 30509
   ```

### Manual tcpdump
```bash
sudo tcpdump -i lo -w capture.pcapng 'udp port 30490 or udp port 30509'
```

Then in separate terminal:
```bash
bash run_wireshark_test.sh
```

---

## Compilation

```bash
cd dynamic_communication
rm -rf build && mkdir build && cd build
cmake -DCMAKE_PREFIX_PATH="/path/to/_install/lib/cmake" ..
make -j4
```

**Build Targets:**
- `main` — Service/Client application
- `dynamic_modular_test` — Unit tests
- `wireshark_capture` — Standalone capture utility (alternative)

---

## Configuration File Format

### Flat Format (rr_service.json, rr_client.json)

```json
{
  "pattern": "request_response",
  "role": "service",              // or "client"
  "rr_fusion": "sum",             // or "avg" (service-side only)
  "service_id": "0x3333",
  "instance_id": "0x0001",
  "method_id": "0x0100",
  "app_service_name": "dyn_service_app",
  "app_client_name": "dyn_client_app"  // Client only
}
```

**For client, add request data:**
```json
{
  "pattern": "request_response",
  "role": "client",
  "v1": 100,                      // First wheel ticks
  "v2": 200                       // Second wheel ticks
}
```

### Communication Pattern Details

- **Service ID:** 0x3333
- **Instance ID:** 0x0001  
- **Method ID:** 0x0100
- **Protocol:** SOME/IP Request/Response
- **Transport:** UDP on port 30509 (unreliable) or TCP on 30510 (reliable)
- **Service Discovery:** Multicast 239.255.0.1:30490 (UDP)

**Request Type:** `WheelTicksRequest {uint32 front_left, uint32 front_right}`
**Response Type:** `VehicleOdometerResponse {uint32 total_ticks}`
**Fusion Methods:** 
  - `sum`: total = v1 + v2
  - `avg`: total = (v1 + v2) / 2

---

## Supported Patterns

1. **request_response** (rr) — Request/Response method calls
2. **fire_and_forget** (ff) — One-way message (no response expected)
3. **event** — Service publishes events clients subscribe to
4. **field** — Getter/Setter with event notifications

---

## Build Errors & Troubleshooting

### "Configuration module could not be loaded!"
→ Check `VSOMEIP_CONFIGURATION` environment variable points to valid JSON

### "Could not find a package configuration file provided by vsomeip3"
→ Set CMAKE_PREFIX_PATH: 
```bash
cmake -DCMAKE_PREFIX_PATH="/path/to/_install/lib/cmake" ..
```

### tcpdump: "permission denied" or no output from capture
→ See Network Capture section above for permission setup

### Service crashes during startup
→ Check that no other process is using ports 30490, 30509, 30510
```bash
lsof -i :30490 -i :30509 -i :30510
```

---

## Development Workflow

### Add New Pattern Type

1. Create `lib/patterns/MyPattern/MyPattern.h|cpp`
2. Add handler in `PatternRuntimeConfig.cpp`
3. Update CMakeLists.txt
4. Create JSON config in `config/`
5. Test with: `./build/main config/my_pattern.json`

### Add New Data Types

Edit `include/types/vehicle_types.hpp` and update serialization in pattern implementations.

---

## Notes

- All paths assume you're in `/home/oussema/Bureau/PFE/pfe-vsomeip-tests -2/pfe-vsomeip-tests/`
- vsomeip3 v3.6.2 is required (binary in `_install/lib/`)
- Build uses C++14 standard (no C++17 features)
- Network tests use localhost loopback (127.0.0.1)

---

Generated: 2026-05-11  
Status: ✓ Working (tested with successful RR communication)
