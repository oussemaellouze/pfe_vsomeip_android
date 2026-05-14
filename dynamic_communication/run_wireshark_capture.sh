#!/usr/bin/env bash
# Capture tcpdump + main (service / client).
# CAPTURE_USE_IP_LAB=1 : deux « hôtes » 10.100.100.1 / .2 (veth+netns) + configs réseau séparées (trafic Wireshark).
# CAPTURE_USE_IP_LAB=0 (défaut) : vsomeip_dynamic_combined_capture.json — une pile locale (pcap souvent vide).

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
DC_DIR="$SCRIPT_DIR"
BASE_DIR="$(cd "$DC_DIR/.." && pwd)"
BUILD_DIR="$DC_DIR/build"
CONFIG_DIR="$DC_DIR/config"
WIRESHARK_DIR="$DC_DIR/wireshark"
INSTALL_DIR="$BASE_DIR/_install"

if [ "$#" -lt 2 ]; then
    echo "Usage: $0 <config.json> <pattern_key> [service|client]"
    echo "Example: $0 ../config/event.json event"
    echo "Env:"
    echo "  CAPTURE_USE_IP_LAB=1 — lab 10.100.100.x (veth+netns) + vsomeip_capture_*_network.json (pcap SOME/IP)"
    echo "  CAPTURE_USE_IP_LAB=0 (défaut) — config combinée locale (127.0.0.1)"
    echo "  VSOMEIP_CAPTURE_VERBOSE=1, WIRESHARK_CAPTURE_QUIET=1"
    exit 1
fi

CONFIG_FILE="$1"
PATTERN_KEY="$2"
ROLE="both"
if [ "$#" -ge 3 ]; then
    ROLE="$3"
fi

CAPTURE_USE_IP_LAB="${CAPTURE_USE_IP_LAB:-0}"

mkdir -p "$WIRESHARK_DIR"
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
PCAPNG_FILE="$WIRESHARK_DIR/capture_$(basename "$CONFIG_FILE" .json)_${PATTERN_KEY}_${ROLE}_${TIMESTAMP}.pcapng"

VSOMEIP_SVC_NET="$CONFIG_DIR/vsomeip_capture_service_network.json"
VSOMEIP_CLI_NET="$CONFIG_DIR/vsomeip_capture_client_network.json"

VSOMEIP_CAPTURE_CFG="$CONFIG_DIR/vsomeip_dynamic_combined_capture.json"
if [ "${VSOMEIP_CAPTURE_VERBOSE:-0}" = "1" ] && [ -f "$CONFIG_DIR/vsomeip_dynamic_combined_capture_verbose.json" ]; then
    VSOMEIP_CAPTURE_CFG="$CONFIG_DIR/vsomeip_dynamic_combined_capture_verbose.json"
fi

export LD_LIBRARY_PATH="$INSTALL_DIR/lib:${LD_LIBRARY_PATH:-}"

if [[ "$CONFIG_FILE" == /* ]]; then
    CONFIG_ABS="$CONFIG_FILE"
else
    CONFIG_ABS="$BUILD_DIR/$CONFIG_FILE"
fi

LAB_NET="10.100.100"
SVC_IP="${LAB_NET}.1"
CLI_IP="${LAB_NET}.2"
IFACE_ROOT="vws0"
IFACE_NS="vws1"
NS="vsw_cap_client"
SVC_PID=""
TCPDUMP_PID=""
TD_SUDO_PID=""
LAB_ACTIVE=0

lab_cleanup() {
    set +e
    [ -n "${SVC_PID:-}" ] && kill "$SVC_PID" 2>/dev/null || true
    [ -n "${TCPDUMP_PID:-}" ] && sudo kill -2 "$TCPDUMP_PID" 2>/dev/null || true
    sleep 1
    sudo ip netns del "$NS" 2>/dev/null || true
    sudo ip link del "$IFACE_ROOT" 2>/dev/null || true
}

mono_cleanup() {
    set +e
    kill "${SVC_PID:-}" 2>/dev/null || true
    sudo kill -2 "${TCPDUMP_PID:-}" 2>/dev/null || true
    sleep 2
    if sudo kill -0 "${TCPDUMP_PID:-}" 2>/dev/null; then
        sudo kill -9 "$TCPDUMP_PID" 2>/dev/null || true
    fi
    if [ "${TD_SUDO_PID:-}" != "${TCPDUMP_PID:-}" ] && kill -0 "${TD_SUDO_PID:-}" 2>/dev/null; then
        kill -2 "$TD_SUDO_PID" 2>/dev/null || true
        sleep 1
        kill -9 "$TD_SUDO_PID" 2>/dev/null || true
    fi
}

cleanup() {
    if [ "$LAB_ACTIVE" = "1" ]; then
        lab_cleanup
    else
        mono_cleanup
    fi
}
trap cleanup EXIT

if ! command -v tcpdump &>/dev/null; then
    echo "[ERREUR] tcpdump n'est pas installé (apt install tcpdump)"
    exit 1
fi

if [ ! -x "$BUILD_DIR/main" ]; then
    echo "[ERREUR] Compilez d'abord: cmake --build $BUILD_DIR"
    exit 1
fi

if [ "${WIRESHARK_CAPTURE_QUIET:-0}" != "1" ]; then
    echo "Capture: $CONFIG_FILE | pattern=$PATTERN_KEY role=$ROLE | pcap=$PCAPNG_FILE"
    echo "CAPTURE_USE_IP_LAB=$CAPTURE_USE_IP_LAB (1=deux réseaux + netns, 0=combiné local)"
fi

cd "$BUILD_DIR"

USE_LAB=0
if [ "$CAPTURE_USE_IP_LAB" = "1" ] && [ "$ROLE" = "both" ] && [ -f "$VSOMEIP_SVC_NET" ] && [ -f "$VSOMEIP_CLI_NET" ]; then
    USE_LAB=1
elif [ "$CAPTURE_USE_IP_LAB" = "1" ] && [ "$ROLE" != "both" ]; then
    echo "[INFO] CAPTURE_USE_IP_LAB=1 nécessite role=both — bascule mode local (CAPTURE_USE_IP_LAB=0)."
    USE_LAB=0
fi

if [ "$USE_LAB" = "1" ]; then
    LAB_ACTIVE=1
    sudo ip link del "$IFACE_ROOT" 2>/dev/null || true
    sudo ip netns del "$NS" 2>/dev/null || true

    sudo ip link add "$IFACE_ROOT" type veth peer name "$IFACE_NS"
    sudo ip addr add "${SVC_IP}/24" dev "$IFACE_ROOT"
    sudo ip link set "$IFACE_ROOT" up

    sudo ip netns add "$NS"
    sudo ip link set "$IFACE_NS" netns "$NS"
    sudo ip netns exec "$NS" ip addr add "${CLI_IP}/24" dev "$IFACE_NS"
    sudo ip netns exec "$NS" ip link set "$IFACE_NS" up
    sudo ip netns exec "$NS" ip link set lo up

    sudo ip route add 224.0.0.0/4 dev "$IFACE_ROOT" metric 50 2>/dev/null || true
    sudo ip netns exec "$NS" ip route add 224.0.0.0/4 dev "$IFACE_NS" metric 50 2>/dev/null || true

    sudo tcpdump -i "$IFACE_ROOT" -s 0 -U -w "$PCAPNG_FILE" \
        'udp port 30490 or udp port 30509 or tcp port 30510 or (ip multicast and udp)' \
        >/dev/null 2>&1 &
    TD_SUDO_PID=$!
    sleep 0.5
    TCPDUMP_PID="$(pgrep -P "$TD_SUDO_PID" 2>/dev/null | head -1)"
    [ -z "$TCPDUMP_PID" ] && TCPDUMP_PID="$TD_SUDO_PID"

    if ! sudo kill -0 "$TCPDUMP_PID" 2>/dev/null && ! kill -0 "$TD_SUDO_PID" 2>/dev/null; then
        echo "[ERREUR] tcpdump n'a pas démarré."
        exit 1
    fi
    [ "${WIRESHARK_CAPTURE_QUIET:-0}" != "1" ] && echo "[1/4] tcpdump sur $IFACE_ROOT (lab $SVC_IP / $CLI_IP)"

    [ "${WIRESHARK_CAPTURE_QUIET:-0}" != "1" ] && echo "[2/4] service ($VSOMEIP_SVC_NET)..."
    env VSOMEIP_CONFIGURATION="$VSOMEIP_SVC_NET" ./main "$CONFIG_FILE" "$PATTERN_KEY" service &
    SVC_PID=$!
    sleep 4

    [ "${WIRESHARK_CAPTURE_QUIET:-0}" != "1" ] && echo "[3/4] client dans netns ($VSOMEIP_CLI_NET)..."
    sudo ip netns exec "$NS" env \
        VSOMEIP_CONFIGURATION="$VSOMEIP_CLI_NET" \
        LD_LIBRARY_PATH="$LD_LIBRARY_PATH" \
        bash -lc "cd '$BUILD_DIR' && exec './main' '$CONFIG_ABS' '$PATTERN_KEY' client"
    CLIENT_EXIT=$?

    [ "${WIRESHARK_CAPTURE_QUIET:-0}" != "1" ] && echo "[4/4] fin capture..."
    sudo kill -2 "$TCPDUMP_PID" 2>/dev/null || true
    sleep 2
    sudo kill -9 "$TCPDUMP_PID" 2>/dev/null || true
    kill "$SVC_PID" 2>/dev/null || true
    sleep 1
else
    LAB_ACTIVE=0
    if [ -f "$VSOMEIP_CAPTURE_CFG" ]; then
        export VSOMEIP_CONFIGURATION="$VSOMEIP_CAPTURE_CFG"
    else
        export VSOMEIP_CONFIGURATION="$CONFIG_DIR/vsomeip_dynamic_combined.json"
        echo "[ATTENTION] $VSOMEIP_CAPTURE_CFG absent — vsomeip_dynamic_combined.json"
    fi

    sudo tcpdump -i any -s 0 -U -w "$PCAPNG_FILE" \
        'udp port 30490 or udp port 30509 or tcp port 30510 or (ip multicast and udp)' \
        >/dev/null 2>&1 &
    TD_SUDO_PID=$!
    sleep 0.5
    TCPDUMP_PID="$(pgrep -P "$TD_SUDO_PID" 2>/dev/null | head -1)"
    [ -z "$TCPDUMP_PID" ] && TCPDUMP_PID="$TD_SUDO_PID"

    sleep 1
    if ! sudo kill -0 "$TCPDUMP_PID" 2>/dev/null && ! kill -0 "$TD_SUDO_PID" 2>/dev/null; then
        echo "[ERREUR] tcpdump n'a pas démarré (sudo / mot de passe)."
        exit 1
    fi

    [ "${WIRESHARK_CAPTURE_QUIET:-0}" != "1" ] && echo "[1/4] tcpdump démarré"

    [ "${WIRESHARK_CAPTURE_QUIET:-0}" != "1" ] && echo "[2/4] service..."
    ./main "$CONFIG_FILE" "$PATTERN_KEY" service &
    SVC_PID=$!
    [ "${WIRESHARK_CAPTURE_QUIET:-0}" != "1" ] && echo "    PID service=$SVC_PID"

    sleep 3

    [ "${WIRESHARK_CAPTURE_QUIET:-0}" != "1" ] && echo "[3/4] client..."
    ./main "$CONFIG_FILE" "$PATTERN_KEY" client
    CLIENT_EXIT=$?

    [ "${WIRESHARK_CAPTURE_QUIET:-0}" != "1" ] && echo "[4/4] fin capture..."
    sudo kill -2 "$TCPDUMP_PID" 2>/dev/null || true
    sleep 2
    if sudo kill -0 "$TCPDUMP_PID" 2>/dev/null; then
        sudo kill -9 "$TCPDUMP_PID" 2>/dev/null || true
    fi
    if [ "$TD_SUDO_PID" != "$TCPDUMP_PID" ] && kill -0 "$TD_SUDO_PID" 2>/dev/null; then
        kill -2 "$TD_SUDO_PID" 2>/dev/null || true
        sleep 1
        kill -9 "$TD_SUDO_PID" 2>/dev/null || true
    fi
    [ "${WIRESHARK_CAPTURE_QUIET:-0}" != "1" ] && echo "    tcpdump arrêté"
    sleep 1
    kill "$SVC_PID" 2>/dev/null || true
fi

PCAP_BYTES=0
if [ -f "$PCAPNG_FILE" ]; then
    PCAP_BYTES=$(stat -c%s "$PCAPNG_FILE" 2>/dev/null || stat -f%z "$PCAPNG_FILE" 2>/dev/null || echo 0)
fi

if [ "$PCAP_BYTES" -gt 256 ] 2>/dev/null; then
    echo "✓ pcap: $PCAPNG_FILE ($(numfmt --to=iec-i --suffix=B "$PCAP_BYTES" 2>/dev/null || echo "${PCAP_BYTES} o"))"
else
    echo "⚠ pcap petit (${PCAP_BYTES} o). Si mode local: normal (Unix). Sinon vérifier SD / délais."
fi

exit "${CLIENT_EXIT:-0}"
