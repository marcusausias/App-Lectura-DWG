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

// Referencia externa declarada por el dibujo: un bloque cuyo contenido vive en
// otro archivo.
struct XrefDeclaration {
  std::string blockName;  // Nombre con el que se inserta dentro del dibujo.
  std::string rawPath;    // Ruta tal cual la escribió AutoCAD, formato Windows.
  bool isOverlay = false;
};

struct ExtractedScene {
  bool ok = false;
  std::string errorMessage;

  // Contenido del espacio modelo.
  std::vector<dwgcore::Entity> entities;
  std::vector<dwgcore::InsertRef> inserts;

  // Definiciones de bloque, listas para aplanar.
  std::map<std::string, dwgcore::BlockDefinition> blocks;

  // Referencias externas que el dibujo declara. Sus bloques quedan sin definir
  // a propósito: hasta que no se resuelve dónde está cada archivo, el aplanado
  // las cuenta como bloques ausentes y la interfaz puede avisar en vez de
  // dibujar el plano incompleto en silencio.
  std::vector<XrefDeclaration> xrefs;

  // Tipos de entidad que todavía no se convierten, con su recuento. Sirve para
  // saber qué falta por implementar mirando planos reales en vez de adivinando.
  std::map<std::string, int> unsupported;
};

// Carga los archivos de las referencias externas y las incorpora al dibujo.
//
// `resolvedPaths` asocia el nombre del bloque de cada xref con la ruta real del
// archivo en el dispositivo. Las que no aparezcan se quedan sin resolver, y
// seguirán apareciendo como bloques ausentes.
//
// Las xrefs pueden contener a su vez otras xrefs; `maxDepth` acota la
// recursión para que una cadena rota no cuelgue la carga.
struct XrefResolution {
  int resolved = 0;
  int failed = 0;
  std::vector<std::string> unresolvedBlocks;
};

XrefResolution mergeXrefs(ExtractedScene& scene,
                          const std::map<std::string, std::string>& resolvedPaths,
                          int maxDepth = 4);

ExtractedScene extractScene(const std::string& path);

}  // namespace dwgapp
