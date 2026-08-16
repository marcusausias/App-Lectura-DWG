// Escena: el plano ya listo para dibujar y medir.
//
// Es lo que se guarda en caché. A partir de aquí no se vuelve a tocar el DWG:
// abrir un plano por segunda vez consiste en leer este bloque y poco más.
#pragma once

#include <string>
#include <vector>

#include "dwgcore/entity.h"
#include "dwgcore/spatial.h"

namespace dwgcore {

struct Scene {
  // Nombres de capa sin repetir. Las entidades apuntan aquí por índice, que es
  // también como se agrupan los lotes de dibujo y como funciona el panel de
  // capas: encender o apagar una es marcar un índice.
  std::vector<std::string> layers;

  std::vector<Entity> entities;
  Bounds bounds;
  SpatialIndex index;
};

// Entidades que realmente cortan el área.
//
// `Scene::index` hace un descarte grueso: devuelve hojas enteras del árbol, así
// que incluye vecinos que no llegan a tocar el área. Para dibujar da igual
// —dibujar de más no se nota—, pero al buscar puntos de enganche o al decidir
// qué se ha tocado hay que afinar, y eso se hace aquí contrastando la caja de
// cada candidato.
void queryEntities(const Scene& scene, const Bounds& area,
                   std::vector<uint32_t>& results);

// Monta la escena a partir de las entidades ya aplanadas: unifica los nombres
// de capa, calcula la extensión total y construye el índice espacial.
Scene buildScene(std::vector<Entity> entities, int nodeCapacity = 16);

}  // namespace dwgcore
