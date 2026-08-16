// Índice espacial estático (R-tree empaquetado por STR).
//
// Se construye una sola vez, cuando el plano se carga, y a partir de ahí no
// cambia. Eso permite empaquetarlo de golpe en vez de insertar entidad a
// entidad: sale un árbol mejor equilibrado, se construye mucho más rápido y
// queda en arrays planos que se pueden volcar a la caché tal cual.
//
// Da servicio a las tres operaciones que tienen que ir sobradas con 200.000
// entidades: recortar por pantalla al dibujar, encontrar qué se ha tocado, y
// buscar los puntos de enganche cercanos al medir.
#pragma once

#include <cstdint>
#include <vector>

#include "dwgcore/entity.h"

namespace dwgcore {

struct IndexNode {
  Bounds bounds;
  uint32_t firstChild = 0;  // Posición en la tabla de enlaces.
  uint32_t childCount = 0;
  uint8_t leaf = 0;
};

class SpatialIndex {
 public:
  // Construye el árbol a partir de las cajas envolventes, en el mismo orden que
  // las entidades. `nodeCapacity` es cuántos hijos entran en un nodo.
  void build(const std::vector<Bounds>& boxes, int nodeCapacity = 16);

  // Índices de las entidades cuya caja corta el área dada. No filtra por
  // geometría real: es un descarte grueso y rápido, y quien llama afina si le
  // hace falta.
  void query(const Bounds& area, std::vector<uint32_t>& results) const;

  bool empty() const { return nodes_.empty(); }
  size_t nodeCount() const { return nodes_.size(); }
  const Bounds& bounds() const { return rootBounds_; }
  uint32_t root() const { return root_; }

  // Acceso plano para poder guardar y restaurar el índice sin reconstruirlo.
  const std::vector<IndexNode>& nodes() const { return nodes_; }
  const std::vector<uint32_t>& links() const { return links_; }
  void restore(std::vector<IndexNode> nodes, std::vector<uint32_t> links,
               uint32_t root);

 private:
  std::vector<IndexNode> nodes_;
  // Tabla de enlaces compartida: en una hoja son índices de entidad, y en un
  // nodo interior son índices de otros nodos.
  std::vector<uint32_t> links_;
  uint32_t root_ = 0;
  Bounds rootBounds_;
};

}  // namespace dwgcore
