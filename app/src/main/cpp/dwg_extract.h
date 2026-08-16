// Extracción de la geometría de un DWG al modelo normalizado del núcleo.
//
// Es la frontera entre LibreDWG y el resto de la app: a partir de aquí ya no
// se habla de DWG, sino de entidades con vértices en coordenadas de mundo.
#pragma once

#include <map>
#include <string>
#include <vector>

#include "dwgcore/flatten.h"

namespace dwgapp {

struct ExtractedScene {
  bool ok = false;
  std::string errorMessage;

  // Contenido del espacio modelo.
  std::vector<dwgcore::Entity> entities;
  std::vector<dwgcore::InsertRef> inserts;

  // Definiciones de bloque, listas para aplanar.
  std::map<std::string, dwgcore::BlockDefinition> blocks;

  // Tipos de entidad que todavía no se convierten, con su recuento. Sirve para
  // saber qué falta por implementar mirando planos reales en vez de adivinando.
  std::map<std::string, int> unsupported;
};

ExtractedScene extractScene(const std::string& path);

}  // namespace dwgapp
