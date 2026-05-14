
# RÉSUMÉ - Qu'est-ce que ce code fait?

## **Vue d'ensemble du projet**

Ce projet implémente une **démonstration complète de communication SOME/IP (Scalable service-Oriented MiddlewarE over IP)** en utilisant des **patterns de communication modernes**:

### **Objectif Principal**
Tester et démontrer la communication client/serveur SOME/IP avec plusieurs patterns:
- **Request/Response (RR)**: Client demande, serveur répond
- **Event**: Serveur publie des événements, clients s'abonnent
- **Field**: Gestion de propriétés (get/set/notify)

### **Cas d'Usage Concret**
**Système de fusion d'odométrie de véhicule**:
- Deux compteurs de roues (v1, v2)
- Client envoie les deux valeurs
- Serveur fusionne (moyenne ou somme)
- Client reçoit la valeur fusionnée

### **Architecture**
```
Configuration JSON
        ↓
     main.cpp
        ↓
PatternRuntimeConfig (parse config)
        ↓
    Patterns spécifiques
        ↓
DynamicCommLibrary (communication SOME/IP)
        ↓
Types sérialisés
        ↓
    Réseau (UDP/TCP)
```

### **Points clés**
1. **Modularité**: Un code pour tous les patterns
2. **Configuration externe**: JSON définit le comportement
3. **Sérialisation**: Conversion C++ ↔ bytes réseau
4. **Deux modes**: Service et Client dans le même binaire
5. **Observable**: Captures Wireshark + interface web

### **Commandes essentielles**
```bash
# Compiler
cd dynamic_communication && mkdir build && cd build && cmake .. && make

# Lancer service
./build/main config/patterns_rr_ex1_service.json

# Lancer client (autre terminal)
./build/main config/patterns_rr_ex1_client.json

# Lancer avec namespaces (pour Wireshark)
bash dynamic_communication/docs/scripts/run_ns_all_patterns_demo.sh

# Visualiser interface web
python3 dynamic_communication/ui/web_ui.py --host 0.0.0.0 --port 8080
# Accéder à http://localhost:8080

# Voir capture Wireshark
wireshark dynamic_communication/docs/ns_someip_all_patterns.pcapng
```
        
## 1. Point d'Entrée: main.cpp

**QUE FAIT CE FICHIER?**
- Point d'entrée du programme
- Charge la configuration JSON du pattern SOME/IP
- Dispatche vers le pattern approprié (RR, Event, Field, etc.)
- Gère les arguments en ligne de commande

**FLUX PRINCIPAL:**
1. Parse arguments (fichier config)
2. Charge PatternRuntimeConfig
3. Sélectionne le pattern selon la config
4. Lance service ET client en parallèle
5. Gère les signaux d'arrêt
        
**EXTRAIT DU CODE:**
```cpp
#include "core/PatternRuntimeConfig.h"

#include "patterns/Event/Event.h"
#include "patterns/Fire_Forget/FireForget.h"
#include "patterns/Field/Field.h"
#include "patterns/Request_Response/RequestResponse.h"

#include <iostream>
#include <string>

static void print_usage(const char *exe) {
    std::cout
        << "Usage:\n"
        << "  " << exe << " <flat_config.json>\n"
        << "      Single JSON with pattern, role, IDs (legacy flat file).\n"
        << "\n"
        << "  " << exe << " <unified.json> <pattern_key> <service|client>\n"
        << "      unified.json must contain top-level \"patterns\" : { ... }.\n"
        << "      pattern_key examples: rr, ff, event, field, rr_complex_list, rr_complex_set, rr_complex_map\n"
        << "      Each pattern object has \"service\" and \"client\" sub-objects (flat keys).\n"
        << "      See config/traffic_all_patterns.json\n"
        << "\n"
        << "Supported keys (flat fragment):\n"
        << "  pattern: request_response | rr | fire_and_forget | ff | event | field\n"
        << "  role: service | client (ignored for unified mode; taken from argv[3])\n"
        << "  rr_use_case: basic_ticks | complex_list | complex_set | complex_map\n"
        << "  rr_fusion (RR only): sum | avg\n"
        << "  service_id, instance_id, method_id, event_id, eventgroup_id, v1, v2\n"
        << "  app_service_name, app_client_name\n";
}
...
```
        
## 2. Configuration Runtime: PatternRuntimeConfig

**QUE FAIT CE FICHIER?**
- Charge et parse le fichier JSON de configuration
- Extrait les paramètres SOME/IP (service ID, method ID, ports)
- Configure le pattern d'exécution (RR, Event, Field)
- Gère les types de données et la sérialisation

**RESPONSABILITÉS:**
- Lire config JSON
- Valider les paramètres
- Fournir une interface d'accès aux données
- Initialiser les endpoints SOME/IP
        
**STRUCTURE:**
```cpp
#pragma once

#include <cstdint>
#include <stdexcept>
#include <string>

namespace dynamic_comm {

enum class PatternKind { RequestResponse, FireAndForget, Event, Field };
enum class RoleKind { Service, Client, Both };
enum class RRFusionKind { Sum, Average };
enum class RRUseCaseKind { BasicTicks, ComplexList, ComplexSet, ComplexMap };

struct PatternRuntimeConfig {
    PatternKind pattern = PatternKind::RequestResponse;
    RoleKind role = RoleKind::Both;
    RRFusionKind rr_fusion = RRFusionKind::Average;
    RRUseCaseKind rr_use_case = RRUseCaseKind::BasicTicks;

    uint32_t service_id = 0x3333;
    uint32_t instance_id = 0x0001;

    uint32_t method_id = 0x0100;

    uint32_t event_id = 0x8001;
...
```
        
## 3. Pattern Request/Response

**QUE FAIT CE PATTERN?**
- Client envoie une REQUÊTE (input data)
- Service traite la requête
- Service envoie une RÉPONSE (output data)

**FLUX DÉTAILLÉ:**
1. Service démarre et s'enregistre sur SOME/IP
2. Client découvre le service via SOME/IP-SD
3. Client sérialise les données (v1, v2)
4. Client envoie via SOME/IP (UDP ou TCP)
5. Service reçoit et désérialise
6. Service calcule la fusion (avg ou sum)
7. Service sérialise la réponse
8. Service renvoie la réponse
9. Client reçoit et affiche le résultat

**TYPES DE DONNÉES:**
- Request: WheelTicksRequest (v1, v2)
- Response: VehicleOdometerResponse (fused_value)
        
## 4. Types de Données: vehicle_types.hpp

**QUE FAIT CE FICHIER?**
- Définit les structures de données SOME/IP
- Sérialisation/désérialisation des types
- Codage BIN (Binary SOME/IP Format)

**STRUCTURES PRINCIPALES:**
- WheelTicksRequest: {v1, v2} (compteurs de roues)
- VehicleOdometerResponse: {fused_value} (odométrie fusionnée)

**SÉRIALISATION:**
Convertit les C++ types en bytes SOME/IP pour le réseau
        
**EXTRAIT:**
```cpp
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
...
```
        
## 5. Logique Métier: vehicle_modules.hpp

**QUE FAIT CE FICHIER?**
- Implémente la logique de fusion des données (business logic)
- Calcule la moyenne (AVERAGE) des compteurs
- Calcule la somme (SUM) des compteurs

**FONCTIONS PRINCIPALES:**
- rr_fusion_average(v1, v2) → (v1+v2)/2
- rr_fusion_sum(v1, v2) → v1+v2
- rr_fusion(v1, v2, method) → sélectionne la bonne fusion

**UTILISATION:**
Appelée côté serveur après réception d'une requête
        
## 6. Fichiers de Configuration JSON

**QUE FAIT CES FICHIERS?**
- Configurent les paramètres SOME/IP
- Définissent les IDs de service, méthode, événement
- Spécifient le transport (UDP, TCP)
- Configurent les ports d'écoute

**FICHIER SERVICE (patterns_rr_ex1_service.json):**
        
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
...
```
            
**FICHIER CLIENT (patterns_rr_ex1_client.json):**
        
```json
{
  "pattern": "request_response",
  "role": "both",
  "rr_fusion": "sum",
  "service_id": "0x3333",
  "instance_id": "0x0001",
  "method_id": "0x0100",
  "app_client_name": "dyn_client_app",
  "v1": 21,
  "v2": 21
}
...
```
            
## 7. Communication SOME/IP: DynamicCommLibrary

**QUE FAIT CE FICHIER?**
- Encapsule la communication SOME/IP
- Gère les endpoints (service et client)
- Sérialise/désérialise les messages
- Gère l'envoi et la réception des données

**CLASSES PRINCIPALES:**
- DynamicCommEndpoint: Endpoint générique SOME/IP
- Service: Crée un endpoint serveur
- Client: Crée un endpoint client

**RESPONSABILITÉS:**
- Créer/configurer sockets (UDP/TCP)
- Envoyer/recevoir messages SOME/IP
- Gérer les IDs service/méthode
- Gérer les timeouts de réponse
        
## 8. Flux d'Exécution Complet

```
PHASE 1: DÉMARRAGE
==================
1. ./build/main config/patterns_rr_ex1_service.json
2. main() → charge PatternRuntimeConfig
3. Détecte pattern = "request_response"
4. Lance run_pattern_request_response()

PHASE 2: INITIALISATION SERVICE
================================
1. Crée DynamicCommEndpoint en mode SERVICE
2. Bind sur le port (ex: 30509)
3. S'enregistre sur SOME/IP-SD
4. Attend les requêtes

PHASE 3: INITIALISATION CLIENT
===============================
1. ./build/main config/patterns_rr_ex1_client.json
2. Crée DynamicCommEndpoint en mode CLIENT
3. Découvre le service via SOME/IP-SD
4. Establish connection au service

PHASE 4: COMMUNICATION (RR)
===========================
1. Client sérialise WheelTicksRequest {v1=100, v2=200}
2. Client envoie via UDP/TCP à service
3. Service reçoit le message
4. Service désérialise {v1=100, v2=200}
5. Service appelle rr_fusion(100, 200)
   → Calcule: (100+200)/2 = 150 OU 100+200 = 300
6. Service sérialise VehicleOdometerResponse {150}
7. Service renvoie la réponse
8. Client reçoit le message
9. Client désérialise {150}
10. Client affiche: "Fused value: 150"

PHASE 5: CAPTURE RÉSEAU (Wireshark)
====================================
1. Lancer: bash docs/scripts/run_ns_all_patterns_demo.sh
2. Lance service dans namespace ns_srv
3. Lance client dans namespace ns_cli
4. tcpdump capture le trafic
5. Wireshark visualise:
   - SOME/IP-SD (discovery)
   - SOME/IP RR (requête/réponse)

PHASE 6: ARRÊT
===============
1. Client termine après avoir reçu
2. Service arrête proprement
3. Libère les ressources
```
        
## 9. Interface Web: web_ui.py

**QUE FAIT CE FICHIER?**
- Lance un serveur Flask
- Serve une interface HTML pour visualiser les résultats
- Affiche les captures Wireshark
- Affiche les logs d'exécution

**UTILISATION:**
```bash
python3 dynamic_communication/ui/web_ui.py --host 0.0.0.0 --port 8080
# Puis accéder à http://localhost:8080
```

**AFFICHAGE:**
- Résumé des captures
- Logs du service et client
- Filtres Wireshark pré-configurés
        