# Configuration Wireshark - Service/Client SOME/IP

## 📋 Fichiers de Configuration

- **rr_service.json** - Configuration du SERVICE (Request/Response)
- **rr_client.json** - Configuration du CLIENT (Request/Response) 
- **vsomeip_dynamic_combined.json** - Configuration VSOMEIP stack
- **run_wireshark_test.sh** - Script de test complet

## 🚀 Démarrage Rapide

### Option 1: Script automatique
```bash
cd /home/oussema/Bureau/PFE/pfe-vsomeip-tests\ -2/pfe-vsomeip-tests/dynamic_communication
bash run_wireshark_test.sh
```

### Option 2: Manuel (service + client séparés)

**Terminal 1 - Service:**
```bash
cd /home/oussema/Bureau/PFE/pfe-vsomeip-tests\ -2/pfe-vsomeip-tests/dynamic_communication/build
export VSOMEIP_CONFIGURATION="../config/vsomeip_dynamic_combined.json"
export LD_LIBRARY_PATH="../../_install/lib:$LD_LIBRARY_PATH"
./main ../config/rr_service.json
```

**Terminal 2 - Client (après ~2-3 secondes):**
```bash
cd /home/oussema/Bureau/PFE/pfe-vsomeip-tests\ -2/pfe-vsomeip-tests/dynamic_communication/build
export VSOMEIP_CONFIGURATION="../config/vsomeip_dynamic_combined.json"
export LD_LIBRARY_PATH="../../_install/lib:$LD_LIBRARY_PATH"
./main ../config/rr_client.json
```

## 📊 Résultat Attendu

```
[ECU SERVICE] Vehicle odometry — fuse by sum: front-left 100 + front-right 200 => fused segment ticks = 300
[VEHICLE CLIENT] Return code: 0x0 | fused segment ticks: 300
[OK] RR done.
```

## 🔍 Capture avec Wireshark

### Pendant que service + client tournent:

1. **Ouvrir Wireshark**
```bash
wireshark &
```

2. **Sélectionner interface:** `lo` (localhost/loopback)

3. **Démarrer la capture:** Cliquez sur le bouton play

4. **Appliquer le filtre:**
```
udp.port == 30490 or udp.port == 30509
```

5. **Observer les paquets:**
   - **SOME/IP-SD** (port 30490): Découverte du service
   - **SOME/IP-RR** (port 30509): Requête et réponse

## 📝 Configuration JSON Complète

### rr_service.json
```json
{
  "pattern": "request_response",
  "role": "service",
  "rr_fusion": "sum",
  "service_id": "0x3333",
  "instance_id": "0x0001",
  "method_id": "0x0100",
  "app_service_name": "dyn_service_app"
}
```

### rr_client.json
```json
{
  "pattern": "request_response",
  "role": "client",
  "rr_fusion": "sum",
  "service_id": "0x3333",
  "instance_id": "0x0001",
  "method_id": "0x0100",
  "app_client_name": "dyn_client_app",
  "v1": 100,
  "v2": 200
}
```

## 🔐 Points Importants

1. **VSOMEIP_CONFIGURATION** doit pointer vers `vsomeip_dynamic_combined.json`
2. **LD_LIBRARY_PATH** doit inclure `_install/lib`
3. **Service doit démarrer avant le client** (délai de ~2-3 sec)
4. **Ports utilisés:**
   - 30490 (UDP) - SOME/IP-SD (Service Discovery)
   - 30509 (UDP) - SOME/IP Unreliable (Requests/Responses)
   - 30510 (TCP) - SOME/IP Reliable (non utilisé ici)

## ❌ Fichiers Supprimés

- ✓ Dossier `ui/` supprimé
- ✓ Dossier `docs/` (permissions insuffisantes)
- ✓ Tous les fichiers JSON inutiles supprimés
- ✓ Gardé seulement: `rr_service.json`, `rr_client.json`, `vsomeip_dynamic_combined.json`

## 📥 Exporter Capture Wireshark

1. Dans Wireshark: File → Export Specified Packets...
2. Format: PCAPNG
3. Filename: `capture_wireshark.pcapng`
4. Destination: `dynamic_communication/config/` ou ailleurs
