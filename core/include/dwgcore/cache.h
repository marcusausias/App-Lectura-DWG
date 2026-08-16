// Caché binaria de la escena.
//
// Procesar un DWG cuesta bastante: leerlo, normalizar la geometría, aplanar los
// bloques y montar el índice. Hacerlo cada vez que se abre un plano en obra no
// tiene sentido, porque el archivo no cambia. La escena se vuelca tal cual a
// disco y la segunda apertura consiste en leer este bloque.
//
// El formato no pretende ser portable entre máquinas: se escribe y se lee en el
// mismo dispositivo. Si cambia la versión del formato, la caché se descarta y
// se vuelve a generar.
#pragma once

#include <cstdint>
#include <string>

#include "dwgcore/scene.h"

namespace dwgcore {

// Datos del archivo original con los que se decide si la caché sigue valiendo.
// Si el DWG se ha vuelto a guardar, cambian el tamaño o la fecha y la caché se
// tira: es preferible reprocesar a dibujar un plano viejo sin avisar.
struct CacheStamp {
  uint64_t sourceSize = 0;
  uint64_t sourceModified = 0;
};

bool writeCache(const std::string& path, const Scene& scene, const CacheStamp& stamp);

// Devuelve false si el archivo no existe, si es de otra versión del formato, si
// está corrupto o si el sello no coincide con el que se le pasa.
bool readCache(const std::string& path, const CacheStamp& expected, Scene& scene);

}  // namespace dwgcore
