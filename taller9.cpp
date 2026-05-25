#include <iostream>
#include <fstream>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
    if (argc != 2) {
        std::cerr << "Uso: " << argv[0] << " <archivo_a_mapear>" << std::endl;
        return 1;
    }

    const char *archivo = argv[1];

    // Abre el archivo en modo de lectura
    int fd = open(archivo, O_RDONLY);
    if (fd == -1) {
        std::cerr << "Error al abrir el archivo" << std::endl;
        return 1;
    }

    // Obtiene el tamaño del archivo
    struct stat info_archivo;
    if (fstat(fd, &info_archivo) == -1) {
        std::cerr << "Error al obtener información del archivo" << std::endl;
        close(fd);
        return 1;
    }
    size_t tam_archivo = info_archivo.st_size;

    // Mapea el archivo en memoria
    void *mapeo = mmap(NULL, tam_archivo, PROT_READ, MAP_PRIVATE, fd, 0);
    if (mapeo == MAP_FAILED) {
        std::cerr << "Error al mapear el archivo en memoria" << std::endl;
        close(fd);
        return 1;
    }

    // Ahora puedes acceder al contenido del archivo utilizando el puntero 'mapeo'
    char *contenido = static_cast<char *>(mapeo);

    // Por ejemplo, imprime el contenido del archivo en la consola
    std::cout.write(contenido, tam_archivo);

    // Libera los recursos y cierra el archivo
    if (munmap(mapeo, tam_archivo) == -1) {
        std::cerr << "Error al desmapear el archivo de la memoria" << std::endl;
    }
    close(fd);
    return 0;
}
