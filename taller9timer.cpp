#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <string>
#include <chrono>
#include <random>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

using namespace std;
using namespace std::chrono;

// ---------------------------------------------------------
// Utilidad para medir tiempo
// ---------------------------------------------------------
class Timer {
private:
    high_resolution_clock::time_point inicio;

public:
    Timer() {
        inicio = high_resolution_clock::now();
    }

    double elapsedSeconds() {
        auto fin = high_resolution_clock::now();
        duration<double> tiempo = fin - inicio;
        return tiempo.count();
    }
};

// ---------------------------------------------------------
// Crear archivo grande con números enteros aleatorios
// ---------------------------------------------------------
void crearArchivoGrande(const string& nombreArchivo, size_t cantidadNumeros) {
    ofstream archivo(nombreArchivo, ios::binary);

    if (!archivo) {
        cerr << "Error: no se pudo crear el archivo " << nombreArchivo << endl;
        exit(1);
    }

    random_device rd;
    mt19937 gen(rd());
    uniform_int_distribution<int> dist(1, 100000000);

    cout << "Creando archivo grande con " << cantidadNumeros << " numeros..." << endl;

    Timer timer;

    for (size_t i = 0; i < cantidadNumeros; i++) {
        int numero = dist(gen);
        archivo.write(reinterpret_cast<char*>(&numero), sizeof(int));
    }

    archivo.close();

    cout << "Archivo creado: " << nombreArchivo << endl;
    cout << "Tiempo de creacion: " << timer.elapsedSeconds() << " segundos" << endl;
}

// ---------------------------------------------------------
// Copiar archivo binario
// ---------------------------------------------------------
void copiarArchivo(const string& origen, const string& destino) {
    ifstream entrada(origen, ios::binary);
    ofstream salida(destino, ios::binary);

    if (!entrada || !salida) {
        cerr << "Error al copiar archivo." << endl;
        exit(1);
    }

    salida << entrada.rdbuf();

    entrada.close();
    salida.close();
}

// ---------------------------------------------------------
// Obtener tamaño del archivo en bytes
// ---------------------------------------------------------
size_t obtenerTamanoArchivo(const string& nombreArchivo) {
    struct stat st;

    if (stat(nombreArchivo.c_str(), &st) != 0) {
        cerr << "Error al obtener tamano del archivo." << endl;
        exit(1);
    }

    return st.st_size;
}

// ---------------------------------------------------------
// Ordenamiento usando mmap: archivo cargado/mapeado en memoria
// ---------------------------------------------------------
void ordenarConMemoriaMapeada(const string& nombreArchivo) {
    cout << "\n=== ORDENAMIENTO CON MEMORIA MAPEADA mmap ===" << endl;

    Timer timerTotal;

    int fd = open(nombreArchivo.c_str(), O_RDWR);

    if (fd == -1) {
        cerr << "Error al abrir archivo para mmap." << endl;
        exit(1);
    }

    size_t tamanoBytes = obtenerTamanoArchivo(nombreArchivo);
    size_t cantidadNumeros = tamanoBytes / sizeof(int);

    Timer timerMapeo;

    void* mapeo = mmap(
        NULL,
        tamanoBytes,
        PROT_READ | PROT_WRITE,
        MAP_SHARED,
        fd,
        0
    );

    if (mapeo == MAP_FAILED) {
        cerr << "Error al mapear archivo en memoria." << endl;
        close(fd);
        exit(1);
    }

    double tiempoMapeo = timerMapeo.elapsedSeconds();

    int* datos = static_cast<int*>(mapeo);

    Timer timerOrdenamiento;

    sort(datos, datos + cantidadNumeros);

    double tiempoOrdenamiento = timerOrdenamiento.elapsedSeconds();

    Timer timerSync;

    if (msync(mapeo, tamanoBytes, MS_SYNC) == -1) {
        cerr << "Advertencia: error al sincronizar cambios con disco." << endl;
    }

    double tiempoSync = timerSync.elapsedSeconds();

    if (munmap(mapeo, tamanoBytes) == -1) {
        cerr << "Error al desmapear memoria." << endl;
    }

    close(fd);

    double tiempoTotal = timerTotal.elapsedSeconds();

    cout << "Archivo ordenado: " << nombreArchivo << endl;
    cout << "Cantidad de numeros: " << cantidadNumeros << endl;
    cout << "Tiempo de mapeo: " << tiempoMapeo << " segundos" << endl;
    cout << "Tiempo de ordenamiento en memoria: " << tiempoOrdenamiento << " segundos" << endl;
    cout << "Tiempo de sincronizacion a disco: " << tiempoSync << " segundos" << endl;
    cout << "Tiempo total memoria mapeada: " << tiempoTotal << " segundos" << endl;
}

// ---------------------------------------------------------
// Escribir vector ordenado en archivo temporal
// ---------------------------------------------------------
string escribirBloqueTemporal(const vector<int>& bloque, int indice) {
    string nombre = "bloque_temp_" + to_string(indice) + ".bin";

    ofstream archivo(nombre, ios::binary);

    if (!archivo) {
        cerr << "Error al crear bloque temporal." << endl;
        exit(1);
    }

    archivo.write(
        reinterpret_cast<const char*>(bloque.data()),
        bloque.size() * sizeof(int)
    );

    archivo.close();

    return nombre;
}

// ---------------------------------------------------------
// Estructura para merge de bloques
// ---------------------------------------------------------
struct NodoMerge {
    int valor;
    int indiceArchivo;

    bool operator>(const NodoMerge& otro) const {
        return valor > otro.valor;
    }
};

// ---------------------------------------------------------
// Ordenamiento externo en disco por bloques
// ---------------------------------------------------------
void ordenarEnDiscoPorBloques(
    const string& archivoEntrada,
    const string& archivoSalida,
    size_t numerosPorBloque
) {
    cout << "\n=== ORDENAMIENTO EXTERNO EN DISCO ===" << endl;

    Timer timerTotal;

    ifstream entrada(archivoEntrada, ios::binary);

    if (!entrada) {
        cerr << "Error al abrir archivo de entrada." << endl;
        exit(1);
    }

    vector<string> archivosTemporales;
    vector<int> bloque;
    bloque.reserve(numerosPorBloque);

    int numero;
    int indiceBloque = 0;

    Timer timerFase1;

    while (entrada.read(reinterpret_cast<char*>(&numero), sizeof(int))) {
        bloque.push_back(numero);

        if (bloque.size() == numerosPorBloque) {
            sort(bloque.begin(), bloque.end());

            string temp = escribirBloqueTemporal(bloque, indiceBloque);
            archivosTemporales.push_back(temp);

            bloque.clear();
            indiceBloque++;
        }
    }

    if (!bloque.empty()) {
        sort(bloque.begin(), bloque.end());

        string temp = escribirBloqueTemporal(bloque, indiceBloque);
        archivosTemporales.push_back(temp);

        bloque.clear();
    }

    entrada.close();

    double tiempoFase1 = timerFase1.elapsedSeconds();

    cout << "Bloques temporales creados: " << archivosTemporales.size() << endl;
    cout << "Tiempo fase 1, leer + ordenar bloques + escribir temporales: "
         << tiempoFase1 << " segundos" << endl;

    Timer timerFase2;

    vector<ifstream*> entradasTemp;

    for (const string& nombre : archivosTemporales) {
        ifstream* archivo = new ifstream(nombre, ios::binary);

        if (!(*archivo)) {
            cerr << "Error al abrir archivo temporal." << endl;
            exit(1);
        }

        entradasTemp.push_back(archivo);
    }

    priority_queue<NodoMerge, vector<NodoMerge>, greater<NodoMerge>> heap;

    for (size_t i = 0; i < entradasTemp.size(); i++) {
        int valor;

        if (entradasTemp[i]->read(reinterpret_cast<char*>(&valor), sizeof(int))) {
            heap.push({valor, static_cast<int>(i)});
        }
    }

    ofstream salida(archivoSalida, ios::binary);

    if (!salida) {
        cerr << "Error al crear archivo de salida." << endl;
        exit(1);
    }

    while (!heap.empty()) {
        NodoMerge actual = heap.top();
        heap.pop();

        salida.write(reinterpret_cast<char*>(&actual.valor), sizeof(int));

        int siguiente;

        if (entradasTemp[actual.indiceArchivo]->read(
            reinterpret_cast<char*>(&siguiente),
            sizeof(int)
        )) {
            heap.push({siguiente, actual.indiceArchivo});
        }
    }

    salida.close();

    for (ifstream* archivo : entradasTemp) {
        archivo->close();
        delete archivo;
    }

    for (const string& nombre : archivosTemporales) {
        remove(nombre.c_str());
    }

    double tiempoFase2 = timerFase2.elapsedSeconds();
    double tiempoTotal = timerTotal.elapsedSeconds();

    cout << "Archivo ordenado en disco: " << archivoSalida << endl;
    cout << "Tiempo fase 2, mezcla de bloques en disco: "
         << tiempoFase2 << " segundos" << endl;
    cout << "Tiempo total ordenamiento externo en disco: "
         << tiempoTotal << " segundos" << endl;
}

// ---------------------------------------------------------
// Verificar si un archivo está ordenado
// ---------------------------------------------------------
bool verificarOrdenado(const string& nombreArchivo) {
    ifstream archivo(nombreArchivo, ios::binary);

    if (!archivo) {
        cerr << "Error al abrir archivo para verificacion." << endl;
        return false;
    }

    int anterior;
    int actual;

    if (!archivo.read(reinterpret_cast<char*>(&anterior), sizeof(int))) {
        return true;
    }

    while (archivo.read(reinterpret_cast<char*>(&actual), sizeof(int))) {
        if (anterior > actual) {
            archivo.close();
            return false;
        }

        anterior = actual;
    }

    archivo.close();
    return true;
}

// ---------------------------------------------------------
// Programa principal
// ---------------------------------------------------------
int main(int argc, char* argv[]) {
    if (argc != 3) {
        cout << "Uso:" << endl;
        cout << "./taller9 <cantidad_numeros> <numeros_por_bloque>" << endl;
        cout << endl;
        cout << "Ejemplo:" << endl;
        cout << "./taller9 10000000 1000000" << endl;
        cout << endl;
        cout << "Eso crea un archivo con 10 millones de enteros." << endl;
        cout << "Cada entero ocupa 4 bytes, por lo tanto son aprox. 40 MB." << endl;
        return 1;
    }

    size_t cantidadNumeros = stoull(argv[1]);
    size_t numerosPorBloque = stoull(argv[2]);

    string archivoOriginal = "datos_original.bin";
    string archivoMemoria = "datos_memoria.bin";
    string archivoDisco = "datos_disco.bin";
    string archivoDiscoOrdenado = "datos_disco_ordenado.bin";

    cout << "==============================================" << endl;
    cout << " Taller 9 - Comparacion Memoria vs Disco" << endl;
    cout << "==============================================" << endl;

    cout << "Cantidad de numeros: " << cantidadNumeros << endl;
    cout << "Tamano aproximado: "
         << (cantidadNumeros * sizeof(int)) / (1024.0 * 1024.0)
         << " MB" << endl;

    crearArchivoGrande(archivoOriginal, cantidadNumeros);

    cout << "\nCopiando archivo para pruebas equivalentes..." << endl;
    copiarArchivo(archivoOriginal, archivoMemoria);
    copiarArchivo(archivoOriginal, archivoDisco);

    ordenarConMemoriaMapeada(archivoMemoria);

    ordenarEnDiscoPorBloques(
        archivoDisco,
        archivoDiscoOrdenado,
        numerosPorBloque
    );

    cout << "\n=== VERIFICACION ===" << endl;

    bool memoriaOK = verificarOrdenado(archivoMemoria);
    bool discoOK = verificarOrdenado(archivoDiscoOrdenado);

    cout << "Archivo ordenado con memoria mapeada: "
         << (memoriaOK ? "OK" : "ERROR") << endl;

    cout << "Archivo ordenado con disco externo: "
         << (discoOK ? "OK" : "ERROR") << endl;

    cout << "\nExperimento finalizado." << endl;

    return 0;
}