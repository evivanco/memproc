#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <map>

std::map<std::string, unsigned long> readMemInfo() {
    std::ifstream meminfo("/proc/meminfo");
    std::map<std::string, unsigned long> meminfo_map;

    if (meminfo.is_open()) {
        std::string line;
        while (std::getline(meminfo, line)) {
            std::istringstream iss(line);
            std::string key;
            unsigned long value;
            std::string unit;

            iss >> key >> value >> unit;
            key.pop_back(); // Elimina el ':' al final de la clave
            meminfo_map[key] = value;
        }
        meminfo.close();
    } else {
        std::cerr << "No se pudo abrir /proc/meminfo" << std::endl;
    }

    return meminfo_map;
}

int main() {
    auto meminfo = readMemInfo();

    std::cout << "Memoria Total: " << meminfo["MemTotal"] << " kB" << std::endl;
    std::cout << "Memoria Libre: " << meminfo["MemFree"] << " kB" << std::endl;
    std::cout << "Memoria en Swap Total: " << meminfo["SwapTotal"] << " kB" << std::endl;
    std::cout << "Memoria en Swap Libre: " << meminfo["SwapFree"] << " kB" << std::endl;

    return 0;
}
