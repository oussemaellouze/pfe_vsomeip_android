#include <iostream>
#include <string>
#include <cstdlib>
#include <ctime>
#include <chrono>
#include <thread>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>

static pid_t tcpdump_pid = 0;
static pid_t service_pid = 0;

void signal_handler(int sig) {
    std::cerr << "\n[CAPTURE] Arrêt en cours..." << std::endl;
    
    if (tcpdump_pid > 0) {
        kill(tcpdump_pid, SIGTERM);
        sleep(1);
        waitpid(tcpdump_pid, nullptr, WNOHANG);
    }
    
    if (service_pid > 0) {
        kill(service_pid, SIGTERM);
        waitpid(service_pid, nullptr, WNOHANG);
    }
    
    exit(0);
}

std::string get_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", localtime(&time));
    return std::string(buffer);
}

int main(int argc, char** argv) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    std::string base_dir = "/home/oussema/Bureau/PFE/pfe-vsomeip-tests -2/pfe-vsomeip-tests";
    std::string dc_dir = base_dir + "/dynamic_communication";
    std::string build_dir = dc_dir + "/build";
    std::string config_dir = dc_dir + "/config";
    std::string wireshark_dir = dc_dir + "/wireshark";
    std::string install_dir = base_dir + "/_install";
    
    std::string timestamp = get_timestamp();
    std::string pcapng_file = wireshark_dir + "/capture_" + timestamp + ".pcapng";
    std::string config_file = config_dir + "/wireshark.json";
    
    std::cout << "=========================================" << std::endl;
    std::cout << "Wireshark Capture - Service/Client RR" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "Config:   " << config_file << std::endl;
    std::cout << "Capture:  " << pcapng_file << std::endl;
    std::cout << "" << std::endl;
    
    // Préparer les variables d'environnement
    std::string vsomeip_config = config_dir + "/vsomeip_dynamic_combined.json";
    std::string ld_lib_path = install_dir + "/lib";
    
    setenv("VSOMEIP_CONFIGURATION", vsomeip_config.c_str(), 1);
    setenv("LD_LIBRARY_PATH", ld_lib_path.c_str(), 1);
    
    // Démarrer tcpdump
    std::cout << "[1/4] Démarrage capture tcpdump..." << std::endl;
    std::string tcpdump_cmd = "tcpdump -i lo -w " + pcapng_file + 
                              " 'udp port 30490 or udp port 30509' >/dev/null 2>&1 &";
    system(tcpdump_cmd.c_str());
    
    // Trouver le PID de tcpdump
    sleep(1);
    system("pidof tcpdump > /tmp/tcpdump.pid 2>/dev/null");
    FILE* pf = fopen("/tmp/tcpdump.pid", "r");
    if (pf) {
        fscanf(pf, "%d", &tcpdump_pid);
        fclose(pf);
        std::cout << "    ✓ tcpdump PID: " << tcpdump_pid << std::endl;
    }
    
    // Démarrer le service
    std::cout << "[2/4] Lancement du SERVICE..." << std::endl;
    service_pid = fork();
    if (service_pid == 0) {
        // Child process - service
        chdir(build_dir.c_str());
        execl("./main", "./main", "../config/wireshark.json", "rr", "service", nullptr);
        exit(1);
    }
    std::cout << "    ✓ Service PID: " << service_pid << std::endl;
    
    // Attendre que le service démarre
    sleep(3);
    
    // Démarrer le client
    std::cout << "[3/4] Lancement du CLIENT..." << std::endl;
    pid_t client_pid = fork();
    if (client_pid == 0) {
        // Child process - client
        chdir(build_dir.c_str());
        execl("./main", "./main", "../config/wireshark.json", "rr", "client", nullptr);
        exit(1);
    }
    std::cout << "    ✓ Client PID: " << client_pid << std::endl;
    
    // Attendre que le client termine
    waitpid(client_pid, nullptr, 0);
    
    std::cout << "[4/4] Arrêt de la capture..." << std::endl;
    sleep(1);
    
    // Arrêter le service
    if (service_pid > 0) {
        kill(service_pid, SIGTERM);
        waitpid(service_pid, nullptr, 0);
    }
    
    // Arrêter tcpdump
    if (tcpdump_pid > 0) {
        kill(tcpdump_pid, SIGTERM);
        sleep(1);
        waitpid(tcpdump_pid, nullptr, WNOHANG);
    }
    
    std::cout << "" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "✓ Capture complétée!" << std::endl;
    std::cout << "=========================================" << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Fichier PCAPNG:" << std::endl;
    std::cout << "  " << pcapng_file << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Ouvrir dans Wireshark:" << std::endl;
    std::cout << "  wireshark " << pcapng_file << std::endl;
    std::cout << "" << std::endl;
    std::cout << "Filtre Wireshark:" << std::endl;
    std::cout << "  udp.port == 30490 or udp.port == 30509" << std::endl;
    
    return 0;
}
