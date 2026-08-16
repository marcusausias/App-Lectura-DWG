#include "dwg_document.h"

#include <cstdio>
#include <cstring>
#include <cstdlib>

#include <dwg.h>
#include <dwg_api.h>

namespace dwgapp {
namespace {

// LibreDWG devuelve un mapa de bits, no un código único. Todo lo que quede por
// debajo de DWG_ERR_CRITICAL son avisos —una clase que no conoce, un valor
// fuera de rango— y el dibujo se ha leído igualmente. Tratar cualquier valor
// distinto de cero como fallo rechazaría archivos perfectamente utilizables:
// los propios ejemplos de AutoCAD 2018 devuelven 68.
bool isFatal(int error) { return error >= DWG_ERR_CRITICAL; }

// Lee un campo de texto de una entidad convirtiéndolo a UTF-8.
//
// Imprescindible: a partir de r2007 el DWG guarda las cadenas en UTF-16LE, y
// leer ese `char*` directamente devuelve solo la primera letra, porque el
// segundo byte es un cero que corta la cadena. Un "C:\OBRA\BASE.dwg" se queda
// en "C" sin dar ningún error. La API dinámica hace la conversión y avisa por
// `isNew` de si ha reservado memoria que haya que liberar.
std::string readText(void* entity, const char* entityName, const char* field) {
  char* text = nullptr;
  int isNew = 0;
  if (!dwg_dynapi_entity_utf8text(entity, entityName, field, &text, &isNew,
                                  nullptr)) {
    return {};
  }
  if (text == nullptr) return {};
  std::string result(text);
  if (isNew) std::free(text);
  return result;
}

// Los seis primeros bytes de todo DWG son su código de versión en ASCII
// ("AC1032" para 2018). Leerlos directamente evita depender de la tabla
// interna de LibreDWG.
std::string readVersionCode(const std::string& path) {
  std::FILE* file = std::fopen(path.c_str(), "rb");
  if (file == nullptr) return {};
  char code[7] = {0};
  const size_t read = std::fread(code, 1, 6, file);
  std::fclose(file);
  if (read != 6) return {};
  return std::string(code);
}

}  // namespace

DocumentSummary summarize(const std::string& path) {
  DocumentSummary summary;
  summary.versionCode = readVersionCode(path);

  Dwg_Data dwg;
  std::memset(&dwg, 0, sizeof(dwg));

  const int error = dwg_read_file(path.c_str(), &dwg);
  summary.libredwgError = error;

  if (isFatal(error)) {
    summary.errorMessage =
        "LibreDWG no pudo leer el archivo (código " + std::to_string(error) + ")";
    dwg_free(&dwg);
    return summary;
  }

  summary.opened = true;
  const char* versionName = dwg_version_type(dwg.header.version);
  summary.versionName = versionName != nullptr ? versionName : "";
  summary.objectCount = static_cast<long>(dwg.num_objects);
  summary.entityCount = static_cast<long>(dwg.num_entities);
  summary.layerCount = static_cast<long>(dwg_get_layer_count(&dwg));

  for (unsigned long i = 0; i < dwg.num_objects; ++i) {
    const Dwg_Object* object = &dwg.object[i];
    if (object->supertype != DWG_SUPERTYPE_OBJECT) continue;
    if (object->fixedtype != DWG_TYPE_BLOCK_HEADER) continue;
    if (object->tio.object == nullptr) continue;

    const Dwg_Object_BLOCK_HEADER* block = object->tio.object->tio.BLOCK_HEADER;
    if (block == nullptr) continue;
    ++summary.blockCount;

    // Una referencia externa es un bloque marcado como xref cuya ruta al
    // archivo externo vive en xref_pname. Ese texto viene tal cual lo escribió
    // AutoCAD, con separadores de Windows, y es lo que habrá que resolver
    // contra la carpeta del proyecto.
    if (!block->blkisxref) continue;

    XrefRef xref;
    void* raw = const_cast<Dwg_Object_BLOCK_HEADER*>(block);
    xref.name = readText(raw, "BLOCK_HEADER", "name");
    xref.path = readText(raw, "BLOCK_HEADER", "xref_pname");
    xref.isOverlay = block->xrefoverlaid != 0;
    summary.xrefs.push_back(std::move(xref));
  }

  dwg_free(&dwg);
  return summary;
}

}  // namespace dwgapp
