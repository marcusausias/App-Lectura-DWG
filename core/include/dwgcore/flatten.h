// Aplanado de bloques: convierte el árbol de INSERT del dibujo en una lista
// plana de entidades en coordenadas de mundo.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "dwgcore/entity.h"
#include "dwgcore/transform.h"

namespace dwgcore {

// Una inserción de un bloque dentro de otro contexto.
struct InsertRef {
  std::string blockName;
  Transform2D transform;
};

// Definición de bloque: su contenido en coordenadas locales del bloque.
struct BlockDefinition {
  std::string name;
  std::vector<Entity> entities;
  std::vector<InsertRef> inserts;
};

struct FlattenOptions {
  // Profundidad máxima de anidamiento. Los planos reales rara vez pasan de 4 o
  // 5 niveles; el límite existe para que un dibujo corrupto no cuelgue la app.
  int maxDepth = 8;

  // Desviación máxima al teselar arcos que una escala no uniforme haya
  // convertido en elipses.
  double maxSagitta = 0.001;
};

struct FlattenResult {
  std::vector<Entity> entities;

  // Bloques que se referencian pero no están definidos. En la práctica son
  // referencias externas sin resolver, y hay que avisar de ellas en vez de
  // dibujar el plano incompleto en silencio.
  std::vector<std::string> missingBlocks;

  // Inserciones descartadas por superar la profundidad máxima o por formar un
  // ciclo (un bloque que acaba conteniéndose a sí mismo).
  int skippedTooDeep = 0;
  int skippedCyclic = 0;
};

// Aplana el espacio modelo. `entities` e `inserts` son el contenido del espacio
// modelo; `blocks` el diccionario de definiciones.
FlattenResult flatten(const std::vector<Entity>& entities,
                      const std::vector<InsertRef>& inserts,
                      const std::map<std::string, BlockDefinition>& blocks,
                      const FlattenOptions& options = {});

}  // namespace dwgcore
