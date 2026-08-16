// Apertura de un DWG y extracción de lo que la app necesita saber de él.
//
// Esta capa aísla a la app de la API de LibreDWG: si algún día hubiera que
// cambiar de motor de lectura, es el único punto que habría que reescribir.
#pragma once

#include <string>
#include <vector>

namespace dwgapp {

// Referencia externa declarada dentro del dibujo.
struct XrefRef {
  std::string name;  // Nombre del bloque, p. ej. "BASE"
  std::string path;  // Ruta original tal cual la guardó AutoCAD, en formato
                     // Windows: "..\\XREF\\BASE.dwg"
  bool isOverlay = false;
};

// Resumen de un dibujo: lo justo para decidir qué hacer con él antes de
// procesarlo entero.
struct DocumentSummary {
  bool opened = false;
  std::string versionCode;   // "AC1032"
  std::string versionName;   // "r2018"
  int libredwgError = 0;     // Bitflags de LibreDWG, con valor informativo
  long objectCount = 0;
  long entityCount = 0;
  long layerCount = 0;
  long blockCount = 0;
  std::vector<XrefRef> xrefs;
  std::string errorMessage;  // Solo cuando opened == false
};

// Lee el archivo y devuelve su resumen. `path` debe ser una ruta real del
// sistema de archivos: LibreDWG no sabe nada de URIs de Android, así que el
// llamante tiene que haber copiado antes el documento a la caché de la app.
DocumentSummary summarize(const std::string& path);

}  // namespace dwgapp
