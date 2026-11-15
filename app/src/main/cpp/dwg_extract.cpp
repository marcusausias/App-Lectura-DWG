#include "dwg_extract.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <set>

#include <dwg.h>
#include <dwg_api.h>

#include "dwgcore/entity.h"
#include "dwgcore/geometry.h"
#include "dwgcore/transform.h"

namespace dwgapp {
namespace {

using dwgcore::Entity;
using dwgcore::EntityType;
using dwgcore::PolyVertex;
using dwgcore::Vec2;
using dwgcore::Vec3;

bool isFatal(int error) { return error >= DWG_ERR_CRITICAL; }

// Desviación máxima al teselar lo que no se puede describir con un bulge.
// Está en unidades de dibujo, así que en un plano en metros son 0,1 mm.
constexpr double kMaxSagitta = 0.0001;

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

// Las entidades planas (arco, círculo, polilínea ligera) guardan sus
// coordenadas en el sistema del objeto, no en el del mundo. Sin esta
// conversión, cualquier geometría hecha con simetría aparece reflejada.
Vec2 toWorld(double x, double y, double z, const BITCODE_3BD& extrusion) {
  const Vec3 normal{extrusion.x, extrusion.y, extrusion.z};
  // Una normal nula significa que el DWG no la trae: se asume la de por defecto.
  if (normal.x == 0.0 && normal.y == 0.0 && normal.z == 0.0) return {x, y};
  const Vec3 world = dwgcore::ocsToWcs({x, y, z}, normal);
  return {world.x, world.y};
}

std::string layerNameOf(Dwg_Data* dwg, const Dwg_Object* object) {
  const Dwg_Object_Entity* common = object->tio.entity;
  if (common == nullptr || common->layer == nullptr) return {};
  Dwg_Object* layerObject = dwg_ref_object(dwg, common->layer);
  if (layerObject == nullptr || layerObject->tio.object == nullptr) return {};
  if (layerObject->fixedtype != DWG_TYPE_LAYER) return {};
  return readText(layerObject->tio.object->tio.LAYER, "LAYER", "name");
}

// Color de AutoCAD a 0xFFRRGGBB, o a uno de los marcadores de dwgcore.
//
// Un color puede venir de tres formas: como índice de la paleta de 256, como
// RGB directo, o como "el de la capa" / "el del bloque". El primer byte del
// campo rgb dice cuál se usa en los DWG modernos; en los antiguos solo existe
// el índice.
uint32_t colorOf(const Dwg_Color& color) {
  int index = color.index;
  const unsigned method = (color.rgb >> 24) & 0xFF;
  switch (method) {
    case 0xC0: return dwgcore::kColorByLayer;
    case 0xC1: return dwgcore::kColorByBlock;
    case 0xC2: return 0xFF000000u | (color.rgb & 0x00FFFFFFu);
    case 0xC3: index = static_cast<int>(color.rgb & 0xFF); break;
    default: break;
  }

  // En la tabla de capas un índice negativo significa "capa apagada", con el
  // color en el valor absoluto.
  if (index < 0) index = -index;
  if (index == 256) return dwgcore::kColorByLayer;
  if (index == 0) return dwgcore::kColorByBlock;
  // El 7 es el color que cambia con el fondo: negro sobre claro, blanco sobre
  // oscuro. No se fija aquí; lo decide quien dibuja.
  if (index == 7 || index > 255) return dwgcore::kColorForeground;

  const Dwg_RGB_Palette* palette = dwg_rgb_palette();
  if (palette == nullptr) return dwgcore::kColorForeground;
  return 0xFF000000u | (static_cast<uint32_t>(palette[index].r) << 16) |
         (static_cast<uint32_t>(palette[index].g) << 8) |
         static_cast<uint32_t>(palette[index].b);
}

// Códigos de control que AutoCAD admite dentro de cualquier texto.
void replaceAll(std::string& text, const std::string& from, const std::string& to) {
  size_t at = 0;
  while ((at = text.find(from, at)) != std::string::npos) {
    text.replace(at, from.size(), to);
    at += to.size();
  }
}

std::string decodePercentCodes(std::string text) {
  if (text.find("%%") == std::string::npos) return text;
  for (const char* code : {"%%c", "%%C"}) replaceAll(text, code, "\xC3\x98");  // Ø
  for (const char* code : {"%%d", "%%D"}) replaceAll(text, code, "\xC2\xB0");  // °
  for (const char* code : {"%%p", "%%P"}) replaceAll(text, code, "\xC2\xB1");  // ±
  for (const char* code : {"%%u", "%%U", "%%o", "%%O", "%%k", "%%K"}) {
    replaceAll(text, code, "");
  }
  replaceAll(text, "%%%", "%");
  return text;
}

// Un MTEXT lleva el formato mezclado con el contenido: `{\fArial|b0;Salón}\P12 m²`.
// Dibujado tal cual es ilegible, así que se quita el formato y se dejan solo el
// texto y los saltos de línea.
std::string stripMTextFormat(const std::string& raw) {
  std::string out;
  out.reserve(raw.size());
  for (size_t i = 0; i < raw.size(); ++i) {
    const char c = raw[i];
    if (c == '{' || c == '}') continue;
    if (c != '\\' || i + 1 >= raw.size()) {
      out.push_back(c);
      continue;
    }

    const char code = raw[++i];
    switch (code) {
      case '\\': case '{': case '}':
        out.push_back(code);
        break;
      case 'P': case 'X':
        out.push_back('\n');
        break;
      case 'N':
        out.push_back('\n');
        break;
      case '~':
        out.push_back(' ');
        break;
      case 'L': case 'l': case 'O': case 'o': case 'K': case 'k':
        break;  // Subrayados y tachados: no llevan argumento.
      case 'S': {
        // Fracción apilada: \S1^2; o \S1/2; o \S1#2;
        const size_t end = raw.find(';', i + 1);
        std::string stacked = raw.substr(i + 1, end == std::string::npos
                                                    ? std::string::npos
                                                    : end - i - 1);
        for (char& ch : stacked) {
          if (ch == '^' || ch == '#') ch = '/';
        }
        out += stacked;
        i = end == std::string::npos ? raw.size() : end;
        break;
      }
      case 'U':
        // \U+00E1: carácter Unicode que el archivo no pudo guardar directo.
        if (i + 5 < raw.size() && raw[i + 1] == '+') {
          const unsigned long cp = std::strtoul(raw.substr(i + 2, 4).c_str(), nullptr, 16);
          if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
          } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
          } else {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
          }
          i += 5;
        }
        break;
      default: {
        // El resto (\f fuente, \H altura, \C color, \A alineación, \W, \Q, \T,
        // \p párrafo…) lleva un argumento que acaba en punto y coma.
        const size_t end = raw.find(';', i + 1);
        i = end == std::string::npos ? raw.size() : end;
        break;
      }
    }
  }
  return decodePercentCodes(out);
}

Entity convertLine(const Dwg_Entity_LINE* line) {
  // LINE guarda sus extremos ya en coordenadas de mundo, a diferencia del
  // resto de entidades planas.
  return dwgcore::makeLine({line->start.x, line->start.y},
                           {line->end.x, line->end.y});
}

Entity convertArc(const Dwg_Entity_ARC* arc) {
  const Vec2 center = toWorld(arc->center.x, arc->center.y, arc->center.z,
                              arc->extrusion);
  return dwgcore::makeArc(center, arc->radius, arc->start_angle, arc->end_angle);
}

Entity convertCircle(const Dwg_Entity_CIRCLE* circle) {
  const Vec2 center = toWorld(circle->center.x, circle->center.y,
                              circle->center.z, circle->extrusion);
  return dwgcore::makeCircle(center, circle->radius);
}

Entity convertLwPolyline(const Dwg_Entity_LWPOLYLINE* polyline) {
  Entity entity;
  entity.type = EntityType::Polyline;
  // El bit 512 del código 70 marca la polilínea como cerrada.
  entity.closed = (polyline->flag & 512) != 0;

  entity.vertices.reserve(polyline->num_points);
  for (unsigned long i = 0; i < polyline->num_points; ++i) {
    PolyVertex vertex;
    vertex.position = toWorld(polyline->points[i].x, polyline->points[i].y,
                              polyline->elevation, polyline->extrusion);
    // num_bulges puede ser menor que num_points: los tramos sin bulge
    // declarado son rectos.
    vertex.bulge = (i < polyline->num_bulges) ? polyline->bulges[i] : 0.0;
    entity.vertices.push_back(vertex);
  }

  dwgcore::updateBounds(entity);
  return entity;
}

Entity convertEllipse(const Dwg_Entity_ELLIPSE* ellipse, double maxSagitta) {
  const Vec2 center = toWorld(ellipse->center.x, ellipse->center.y,
                              ellipse->center.z, ellipse->extrusion);
  // sm_axis es el vector del centro al extremo del eje mayor, relativo al
  // centro, así que se rota pero no se traslada.
  const Vec2 majorTip = toWorld(ellipse->sm_axis.x, ellipse->sm_axis.y,
                                ellipse->sm_axis.z, ellipse->extrusion);
  const Vec2 majorAxis = majorTip - toWorld(0.0, 0.0, 0.0, ellipse->extrusion);

  return dwgcore::makeEllipse(center, majorAxis, ellipse->axis_ratio,
                              ellipse->start_angle, ellipse->end_angle,
                              maxSagitta);
}

Entity convertSpline(const Dwg_Entity_SPLINE* spline, double maxSagitta) {
  // El DWG puede describir una spline por puntos de control o por puntos de
  // paso. Solo la primera forma es una B-spline evaluable; la segunda se
  // aproxima uniendo los puntos por los que pasa, que es lo que se ve dibujado.
  if (spline->num_ctrl_pts >= 2 && spline->num_knots > 0) {
    std::vector<Vec2> control;
    control.reserve(spline->num_ctrl_pts);
    for (unsigned long i = 0; i < spline->num_ctrl_pts; ++i) {
      control.push_back({spline->ctrl_pts[i].x, spline->ctrl_pts[i].y});
    }

    std::vector<double> knots;
    knots.reserve(spline->num_knots);
    for (unsigned long i = 0; i < spline->num_knots; ++i) {
      knots.push_back(spline->knots[i]);
    }

    const int degree = spline->degree > 0 ? spline->degree : 3;

    // El número de muestras se saca del tamaño de la curva, no de cuántos
    // puntos de control tenga. Contar puntos de control reparte el mismo
    // detalle a una spline de 70 metros que a una de 7 milímetros, y en un
    // plano con cientos de curvas pequeñas eso es memoria y trabajo de dibujo
    // tirados a la basura.
    //
    // Para un tramo de longitud L partido en n, la separación respecto a la
    // curva va como L²/(8·R·n²). Tomando el radio del orden de L queda
    // n ≈ √(L / 8ε), que es lo que se usa aquí.
    double controlLength = 0.0;
    for (size_t i = 1; i < control.size(); ++i) {
      controlLength += dwgcore::distance(control[i - 1], control[i]);
    }
    const int estimated = static_cast<int>(
        std::ceil(std::sqrt(controlLength / (8.0 * maxSagitta))));
    const int samples = std::clamp(estimated, 8, 256);

    std::vector<Vec2> sampled =
        dwgcore::tessellateBSpline(control, knots, degree, samples);

    std::vector<PolyVertex> vertices;
    vertices.reserve(sampled.size());
    for (const Vec2& point : sampled) vertices.push_back({point, 0.0});

    Entity entity = dwgcore::makePolyline(std::move(vertices), spline->closed_b);
    entity.type = EntityType::Spline;
    entity.approximated = true;
    return entity;
  }

  std::vector<PolyVertex> vertices;
  vertices.reserve(spline->num_fit_pts);
  for (unsigned long i = 0; i < spline->num_fit_pts; ++i) {
    vertices.push_back({{spline->fit_pts[i].x, spline->fit_pts[i].y}, 0.0});
  }
  Entity entity = dwgcore::makePolyline(std::move(vertices), spline->closed_b);
  entity.type = EntityType::Spline;
  entity.approximated = true;
  (void)maxSagitta;
  return entity;
}

Entity convertPoint(const Dwg_Entity_POINT* point) {
  Entity entity;
  entity.type = EntityType::Point;
  entity.vertices = {{toWorld(point->x, point->y, point->z, point->extrusion), 0.0}};
  dwgcore::updateBounds(entity);
  return entity;
}

Entity convertSolid(const Dwg_Entity_SOLID* solid) {
  // Los cuatro vértices de un SOLID no van en orden de recorrido: el tercero y
  // el cuarto están cruzados. Usarlos tal cual dibuja un lazo con forma de
  // reloj de arena en vez del cuadrilátero relleno.
  const double z = solid->elevation;
  std::vector<PolyVertex> vertices = {
      {toWorld(solid->corner1.x, solid->corner1.y, z, solid->extrusion), 0.0},
      {toWorld(solid->corner2.x, solid->corner2.y, z, solid->extrusion), 0.0},
      {toWorld(solid->corner4.x, solid->corner4.y, z, solid->extrusion), 0.0},
      {toWorld(solid->corner3.x, solid->corner3.y, z, solid->extrusion), 0.0},
  };
  Entity entity = dwgcore::makePolyline(std::move(vertices), /*closed=*/true);
  entity.type = EntityType::Hatch;  // Se dibuja relleno, como un sombreado.
  return entity;
}

Entity convertText(Dwg_Entity_TEXT* text) {
  Entity entity;
  entity.type = EntityType::Text;
  entity.vertices = {
      {toWorld(text->ins_pt.x, text->ins_pt.y, text->elevation, text->extrusion),
       0.0}};
  entity.text = decodePercentCodes(readText(text, "TEXT", "text_value"));
  entity.textHeight = text->height;
  entity.textRotation = text->rotation;
  dwgcore::updateBounds(entity);
  return entity;
}

Entity convertMText(Dwg_Entity_MTEXT* mtext) {
  Entity entity;
  entity.type = EntityType::Text;
  entity.vertices = {{toWorld(mtext->ins_pt.x, mtext->ins_pt.y, mtext->ins_pt.z,
                              mtext->extrusion),
                      0.0}};
  entity.text = stripMTextFormat(readText(mtext, "MTEXT", "text"));
  entity.textHeight = mtext->text_height;
  // MTEXT no guarda un ángulo, sino el vector que marca la dirección del texto.
  entity.textRotation = std::atan2(mtext->x_axis_dir.y, mtext->x_axis_dir.x);

  // El punto de un MTEXT suele ser su esquina superior, mientras que el resto
  // de la cadena trata la posición como la línea base de la primera línea. Se
  // baja una altura (o media, si se ancla por el centro) para que el texto no
  // salga un renglón por encima de donde está en el plano.
  const int attachment = static_cast<int>(mtext->attachment);
  double drop = 0.0;
  if (attachment >= 1 && attachment <= 3) drop = entity.textHeight;
  else if (attachment >= 4 && attachment <= 6) drop = entity.textHeight * 0.5;
  if (drop != 0.0) {
    entity.vertices[0].position.x += std::sin(entity.textRotation) * drop;
    entity.vertices[0].position.y -= std::cos(entity.textRotation) * drop;
  }
  dwgcore::updateBounds(entity);
  return entity;
}

Entity convertAttrib(Dwg_Entity_ATTRIB* attrib) {
  Entity entity;
  entity.type = EntityType::Text;
  entity.vertices = {{toWorld(attrib->ins_pt.x, attrib->ins_pt.y,
                              attrib->elevation, attrib->extrusion),
                      0.0}};
  entity.text = decodePercentCodes(readText(attrib, "ATTRIB", "text_value"));
  entity.textHeight = attrib->height;
  entity.textRotation = attrib->rotation;
  dwgcore::updateBounds(entity);
  return entity;
}

Entity convertTrace(const Dwg_Entity_TRACE* trace) {
  // Mismo orden cruzado de vértices que SOLID.
  const double z = trace->elevation;
  std::vector<PolyVertex> vertices = {
      {toWorld(trace->corner1.x, trace->corner1.y, z, trace->extrusion), 0.0},
      {toWorld(trace->corner2.x, trace->corner2.y, z, trace->extrusion), 0.0},
      {toWorld(trace->corner4.x, trace->corner4.y, z, trace->extrusion), 0.0},
      {toWorld(trace->corner3.x, trace->corner3.y, z, trace->extrusion), 0.0},
  };
  Entity entity = dwgcore::makePolyline(std::move(vertices), /*closed=*/true);
  entity.type = EntityType::Hatch;
  return entity;
}

Entity convert3dFace(const Dwg_Entity__3DFACE* face) {
  // Se proyecta sobre el plano: la app es de planta, no de volumen.
  std::vector<PolyVertex> vertices = {
      {{face->corner1.x, face->corner1.y}, 0.0},
      {{face->corner2.x, face->corner2.y}, 0.0},
      {{face->corner3.x, face->corner3.y}, 0.0},
      {{face->corner4.x, face->corner4.y}, 0.0},
  };
  return dwgcore::makePolyline(std::move(vertices), /*closed=*/true);
}

Entity convertLeader(const Dwg_Entity_LEADER* leader) {
  std::vector<PolyVertex> vertices;
  vertices.reserve(leader->num_points);
  for (unsigned long i = 0; i < leader->num_points; ++i) {
    vertices.push_back({{leader->points[i].x, leader->points[i].y}, 0.0});
  }
  return dwgcore::makePolyline(std::move(vertices), /*closed=*/false);
}

// Contornos de un sombreado.
//
// El relleno no se dibuja —un patrón denso multiplica por diez el peso del
// plano sin aportar nada a la medición—, pero el contorno sí: es justo la
// superficie de la estancia, del forjado o del pavimento, y con "Seguir" se
// mide de un toque.
void convertHatch(const Dwg_Entity_HATCH* hatch, std::vector<Entity>& out) {
  constexpr double kTwoPi = 6.283185307179586;

  for (unsigned long p = 0; p < hatch->num_paths; ++p) {
    const Dwg_HATCH_Path& path = hatch->paths[p];
    std::vector<PolyVertex> vertices;
    bool approximated = false;

    const auto world = [&](double x, double y) {
      return toWorld(x, y, hatch->elevation, hatch->extrusion);
    };

    if ((path.flag & 2) != 0) {
      if (path.polyline_paths == nullptr) continue;
      vertices.reserve(path.num_segs_or_paths);
      for (unsigned long i = 0; i < path.num_segs_or_paths; ++i) {
        const Dwg_HATCH_PolylinePath& point = path.polyline_paths[i];
        vertices.push_back({world(point.point.x, point.point.y),
                            path.bulges_present ? point.bulge : 0.0});
      }
    } else {
      if (path.segs == nullptr) continue;
      for (unsigned long i = 0; i < path.num_segs_or_paths; ++i) {
        const Dwg_HATCH_PathSeg& seg = path.segs[i];
        switch (seg.curve_type) {
          case 1:
            vertices.push_back({world(seg.first_endpoint.x, seg.first_endpoint.y), 0.0});
            if (i + 1 == path.num_segs_or_paths) {
              vertices.push_back(
                  {world(seg.second_endpoint.x, seg.second_endpoint.y), 0.0});
            }
            break;
          case 2: {
            // Un arco horario guarda sus ángulos reflejados respecto al eje X.
            double sweep = std::fmod(seg.end_angle - seg.start_angle, kTwoPi);
            if (sweep <= 0.0) sweep += kTwoPi;
            const double sign = seg.is_ccw ? 1.0 : -1.0;
            const double a0 = sign * seg.start_angle;
            const double a1 = sign * seg.end_angle;
            const double bulge = sign * std::tan(sweep / 4.0);
            vertices.push_back({world(seg.center.x + seg.radius * std::cos(a0),
                                      seg.center.y + seg.radius * std::sin(a0)),
                                bulge});
            // El extremo se añade siempre: si el siguiente tramo empieza en el
            // mismo punto queda un vértice repetido, que no dibuja ni mide nada.
            vertices.push_back({world(seg.center.x + seg.radius * std::cos(a1),
                                      seg.center.y + seg.radius * std::sin(a1)),
                                0.0});
            break;
          }
          case 3: {
            // Arco de elipse: se tesela, y la medida pasa a ser aproximada.
            const double major = std::hypot(seg.endpoint.x, seg.endpoint.y);
            const double rotation = std::atan2(seg.endpoint.y, seg.endpoint.x);
            const double minor = major * seg.minor_major_ratio;
            double sweep = std::fmod(seg.end_angle - seg.start_angle, kTwoPi);
            if (sweep <= 0.0) sweep += kTwoPi;
            const double sign = seg.is_ccw ? 1.0 : -1.0;
            const int steps = 32;
            for (int k = 0; k <= steps; ++k) {
              const double t = sign * (seg.start_angle + sweep * k / steps);
              const double ex = major * std::cos(t);
              const double ey = minor * std::sin(t);
              vertices.push_back(
                  {world(seg.center.x + ex * std::cos(rotation) - ey * std::sin(rotation),
                         seg.center.y + ex * std::sin(rotation) + ey * std::cos(rotation)),
                   0.0});
            }
            approximated = true;
            break;
          }
          case 4: {
            if (seg.num_fitpts > 0 && seg.fitpts != nullptr) {
              for (unsigned long k = 0; k < seg.num_fitpts; ++k) {
                vertices.push_back({world(seg.fitpts[k].x, seg.fitpts[k].y), 0.0});
              }
            } else if (seg.control_points != nullptr && seg.knots != nullptr &&
                       seg.num_control_points >= 2) {
              std::vector<Vec2> control;
              for (unsigned long k = 0; k < seg.num_control_points; ++k) {
                control.push_back({seg.control_points[k].point.x,
                                   seg.control_points[k].point.y});
              }
              std::vector<double> knots(seg.knots, seg.knots + seg.num_knots);
              const int degree = seg.degree > 0 ? static_cast<int>(seg.degree) : 3;
              for (const Vec2& point :
                   dwgcore::tessellateBSpline(control, knots, degree, 32)) {
                vertices.push_back({world(point.x, point.y), 0.0});
              }
            }
            approximated = true;
            break;
          }
          default:
            break;
        }
      }
    }

    if (vertices.size() < 2) continue;
    // Si el recorrido vuelve al punto de partida, sobra el último vértice: la
    // polilínea ya se marca como cerrada.
    if (vertices.size() > 2 &&
        dwgcore::distance(vertices.front().position, vertices.back().position) <
            1e-9 &&
        vertices.back().bulge == 0.0) {
      vertices.pop_back();
    }

    Entity entity = dwgcore::makePolyline(std::move(vertices), /*closed=*/true);
    entity.type = EntityType::Hatch;
    entity.approximated = approximated;
    out.push_back(std::move(entity));
  }
}

// Los vértices de una POLYLINE_2D no viven dentro de ella: son entidades
// VERTEX_2D independientes que la tienen como propietaria. Hay que recogerlas
// aparte y emparejarlas por el handle del propietario.
using VertexMap = std::map<unsigned long, std::vector<PolyVertex>>;

Entity convertPolyline2D(const Dwg_Entity_POLYLINE_2D* polyline,
                         std::vector<PolyVertex> vertices) {
  // Bit 1 del código 70.
  const bool closed = (polyline->flag & 1) != 0;
  for (PolyVertex& vertex : vertices) {
    vertex.position = toWorld(vertex.position.x, vertex.position.y,
                              polyline->elevation, polyline->extrusion);
  }
  return dwgcore::makePolyline(std::move(vertices), closed);
}

// Todas las variantes de cota comparten la misma cabecera de campos, definida
// por la macro DIMENSION_COMMON de LibreDWG, de modo que el handle del bloque
// se puede leer a través de cualquiera de ellas.
bool isDimension(int fixedtype) {
  switch (fixedtype) {
    case DWG_TYPE_DIMENSION_ORDINATE:
    case DWG_TYPE_DIMENSION_LINEAR:
    case DWG_TYPE_DIMENSION_ALIGNED:
    case DWG_TYPE_DIMENSION_ANG3PT:
    case DWG_TYPE_DIMENSION_ANG2LN:
    case DWG_TYPE_DIMENSION_RADIUS:
    case DWG_TYPE_DIMENSION_DIAMETER:
      return true;
    default:
      return false;
  }
}

std::string dimensionBlockName(Dwg_Data* dwg, const Dwg_Object* object) {
  const auto* dimension = reinterpret_cast<const Dwg_DIMENSION_common*>(
      object->tio.entity->tio.DIMENSION_LINEAR);
  if (dimension == nullptr || dimension->block == nullptr) return {};

  Dwg_Object* header = dwg_ref_object(dwg, dimension->block);
  if (header == nullptr || header->tio.object == nullptr) return {};
  if (header->fixedtype != DWG_TYPE_BLOCK_HEADER) return {};
  return readText(header->tio.object->tio.BLOCK_HEADER, "BLOCK_HEADER", "name");
}

// Nombre del bloque al que apunta un INSERT.
std::string blockNameOf(Dwg_Data* dwg, const Dwg_Entity_INSERT* insert) {
  if (insert->block_header == nullptr) return {};
  Dwg_Object* header = dwg_ref_object(dwg, insert->block_header);
  if (header == nullptr || header->tio.object == nullptr) return {};
  if (header->fixedtype != DWG_TYPE_BLOCK_HEADER) return {};
  return readText(header->tio.object->tio.BLOCK_HEADER, "BLOCK_HEADER", "name");
}

dwgcore::InsertRef convertInsert(Dwg_Data* dwg, const Dwg_Entity_INSERT* insert) {
  dwgcore::InsertRef ref;
  ref.blockName = blockNameOf(dwg, insert);

  const Vec2 origin =
      toWorld(insert->ins_pt.x, insert->ins_pt.y, insert->ins_pt.z,
              insert->extrusion);

  // Una escala de cero deja el bloque invisible y además haría degenerar la
  // transformada, así que se trata como escala unidad.
  const double scaleX = insert->scale.x != 0.0 ? insert->scale.x : 1.0;
  const double scaleY = insert->scale.y != 0.0 ? insert->scale.y : 1.0;

  ref.transform =
      dwgcore::insertTransform(origin, scaleX, scaleY, insert->rotation);
  return ref;
}

// Marcadores estructurales que aparecen en la lista de entidades pero no
// dibujan nada: delimitan bloques y secuencias. Contarlos como "pendientes de
// implementar" solo ensucia el informe de lo que falta de verdad.
bool isStructuralMarker(int fixedtype) {
  switch (fixedtype) {
    case DWG_TYPE_BLOCK:
    case DWG_TYPE_ENDBLK:
    case DWG_TYPE_SEQEND:
    case DWG_TYPE_ATTDEF:
    case DWG_TYPE_ATTRIB:
    // Los vértices ya se han consumido al montar su polilínea.
    case DWG_TYPE_VERTEX_2D:
    case DWG_TYPE_VERTEX_3D:
      return true;
    default:
      return false;
  }
}

// Handle del propietario de una entidad, que indica en qué bloque vive.
unsigned long ownerHandleOf(Dwg_Data* dwg, const Dwg_Object* object) {
  const Dwg_Object_Entity* common = object->tio.entity;
  if (common == nullptr || common->ownerhandle == nullptr) return 0;
  Dwg_Object* owner = dwg_ref_object(dwg, common->ownerhandle);
  return owner != nullptr ? static_cast<unsigned long>(owner->handle.value) : 0;
}

}  // namespace

ExtractedScene extractScene(const std::string& path) {
  ExtractedScene scene;

  Dwg_Data dwg;
  std::memset(&dwg, 0, sizeof(dwg));
  const int error = dwg_read_file(path.c_str(), &dwg);
  if (isFatal(error)) {
    scene.errorMessage =
        "LibreDWG no pudo leer el archivo (código " + std::to_string(error) + ")";
    dwg_free(&dwg);
    return scene;
  }

  // Tabla de capas: color y si estaba apagada o congelada.
  for (unsigned long i = 0; i < dwg.num_objects; ++i) {
    const Dwg_Object* object = &dwg.object[i];
    if (object->supertype != DWG_SUPERTYPE_OBJECT) continue;
    if (object->fixedtype != DWG_TYPE_LAYER) continue;
    if (object->tio.object == nullptr) continue;
    Dwg_Object_LAYER* layer = object->tio.object->tio.LAYER;
    if (layer == nullptr) continue;

    dwgcore::LayerStyle style;
    const uint32_t color = colorOf(layer->color);
    style.color = color >= 0xFF000000u ? color : dwgcore::kColorForeground;
    style.hidden = layer->frozen || layer->off || layer->color.index < 0;
    scene.layers[readText(layer, "LAYER", "name")] = style;
  }

  // Primera pasada: nombre de cada bloque por su handle, para saber después a
  // cuál pertenece cada entidad.
  std::map<unsigned long, std::string> blockNameByHandle;
  for (unsigned long i = 0; i < dwg.num_objects; ++i) {
    const Dwg_Object* object = &dwg.object[i];
    if (object->supertype != DWG_SUPERTYPE_OBJECT) continue;
    if (object->fixedtype != DWG_TYPE_BLOCK_HEADER) continue;
    if (object->tio.object == nullptr) continue;

    Dwg_Object_BLOCK_HEADER* header = object->tio.object->tio.BLOCK_HEADER;
    if (header == nullptr) continue;

    const std::string name = readText(header, "BLOCK_HEADER", "name");
    blockNameByHandle[static_cast<unsigned long>(object->handle.value)] = name;

    // Las referencias externas se declaran como bloque, pero su contenido está
    // en otro archivo. No se les crea definición: así el aplanado las cuenta
    // como bloque ausente y la interfaz puede avisar en vez de dibujar el plano
    // incompleto en silencio.
    if (header->blkisxref) {
      if (!name.empty()) {
        XrefDeclaration declaration;
        declaration.blockName = name;
        declaration.rawPath = readText(header, "BLOCK_HEADER", "xref_pname");
        declaration.isOverlay = header->xrefoverlaid != 0;
        scene.xrefs.push_back(std::move(declaration));
      }
    }

    if (!header->blkisxref && !name.empty()) {
      dwgcore::BlockDefinition definition;
      definition.name = name;
      scene.blocks[name] = std::move(definition);
    }
  }

  // Vértices de las polilíneas antiguas, agrupados por la polilínea que los
  // posee. Se guardan en el orden en que aparecen en el archivo, que es el
  // orden de recorrido de la polilínea.
  VertexMap vertexOwners;
  for (unsigned long i = 0; i < dwg.num_objects; ++i) {
    Dwg_Object* object = &dwg.object[i];
    if (object->supertype != DWG_SUPERTYPE_ENTITY) continue;
    if (object->tio.entity == nullptr) continue;

    if (object->fixedtype == DWG_TYPE_VERTEX_3D) {
      const Dwg_Entity_VERTEX_3D* vertex = object->tio.entity->tio.VERTEX_3D;
      if (vertex == nullptr) continue;
      vertexOwners[ownerHandleOf(&dwg, object)].push_back(
          {{vertex->point.x, vertex->point.y}, 0.0});
      continue;
    }
    if (object->fixedtype != DWG_TYPE_VERTEX_2D) continue;

    const Dwg_Entity_VERTEX_2D* vertex = object->tio.entity->tio.VERTEX_2D;
    if (vertex == nullptr) continue;

    // La conversión a mundo se hace después, con la extrusión de la polilínea:
    // aquí solo se guardan las coordenadas locales.
    vertexOwners[ownerHandleOf(&dwg, object)].push_back(
        {{vertex->point.x, vertex->point.y}, vertex->bulge});
  }

  const Dwg_Object* modelSpace = dwg_model_space_object(&dwg);
  const unsigned long modelSpaceHandle =
      modelSpace != nullptr ? static_cast<unsigned long>(modelSpace->handle.value)
                            : 0;

  // Inserciones colocadas directamente en el espacio modelo, para saber de
  // cuáles se pueden recoger los atributos.
  std::set<unsigned long> modelSpaceInserts;
  for (unsigned long i = 0; i < dwg.num_objects; ++i) {
    Dwg_Object* object = &dwg.object[i];
    if (object->supertype != DWG_SUPERTYPE_ENTITY) continue;
    if (object->fixedtype != DWG_TYPE_INSERT) continue;
    if (object->tio.entity == nullptr) continue;
    const unsigned long owner = ownerHandleOf(&dwg, object);
    if (owner == modelSpaceHandle || owner == 0) {
      modelSpaceInserts.insert(static_cast<unsigned long>(object->handle.value));
    }
  }

  // Segunda pasada: convertir entidades y repartirlas entre el espacio modelo y
  // los bloques a los que pertenecen.
  for (unsigned long i = 0; i < dwg.num_objects; ++i) {
    Dwg_Object* object = &dwg.object[i];
    if (object->supertype != DWG_SUPERTYPE_ENTITY) continue;
    if (object->tio.entity == nullptr) continue;

    // Lo que el proyectista marcó como invisible no se dibuja ni se mide.
    if ((object->tio.entity->invisible & 1) != 0) continue;

    const unsigned long owner = ownerHandleOf(&dwg, object);
    const bool inModelSpace = (owner == modelSpaceHandle) || (owner == 0);

    // El valor de un atributo —el nombre de una estancia, el número de una
    // puerta— cuelga de la inserción, no del bloque, y ya viene colocado en
    // coordenadas del dibujo. Solo se recogen los de inserciones hechas
    // directamente en el espacio modelo: dentro de un bloque anidado harían
    // falta las transformadas de todos sus contenedores.
    if (object->fixedtype == DWG_TYPE_ATTRIB) {
      Dwg_Entity_ATTRIB* attrib = object->tio.entity->tio.ATTRIB;
      if (attrib == nullptr || (attrib->flags & 1) != 0) continue;
      if (modelSpaceInserts.count(owner) == 0) continue;
      Entity text = convertAttrib(attrib);
      if (text.text.empty()) continue;
      text.id = static_cast<uint64_t>(object->handle.value);
      text.layer = layerNameOf(&dwg, object);
      text.color = colorOf(object->tio.entity->color);
      scene.entities.push_back(std::move(text));
      continue;
    }

    std::string ownerBlock;
    if (!inModelSpace) {
      const auto found = blockNameByHandle.find(owner);
      // Una entidad cuyo bloque no está en la tabla no se puede colocar: se
      // descarta en vez de dibujarla en el sitio equivocado.
      if (found == blockNameByHandle.end()) continue;
      ownerBlock = found->second;
      if (scene.blocks.find(ownerBlock) == scene.blocks.end()) continue;
    }

    // Una cota no guarda sus líneas, flechas y texto: los guarda en un bloque
    // anónimo (*D1, *D2…) al que apunta. Tratándola como una inserción de ese
    // bloque, las cotas se dibujan reutilizando el mismo aplanado que el resto.
    if (isDimension(object->fixedtype)) {
      const std::string blockName = dimensionBlockName(&dwg, object);
      if (blockName.empty()) {
        scene.unsupported[object->name != nullptr ? object->name : "?"] += 1;
        continue;
      }
      dwgcore::InsertRef ref;
      ref.blockName = blockName;
      ref.layer = layerNameOf(&dwg, object);
      ref.color = colorOf(object->tio.entity->color);
      if (inModelSpace) {
        scene.inserts.push_back(std::move(ref));
      } else {
        scene.blocks[ownerBlock].inserts.push_back(std::move(ref));
      }
      continue;
    }

    if (object->fixedtype == DWG_TYPE_INSERT) {
      dwgcore::InsertRef ref = convertInsert(&dwg, object->tio.entity->tio.INSERT);
      if (ref.blockName.empty()) continue;
      ref.layer = layerNameOf(&dwg, object);
      ref.color = colorOf(object->tio.entity->color);
      if (inModelSpace) {
        scene.inserts.push_back(std::move(ref));
      } else {
        scene.blocks[ownerBlock].inserts.push_back(std::move(ref));
      }
      continue;
    }

    if (object->fixedtype == DWG_TYPE_HATCH) {
      std::vector<Entity> contours;
      if (object->tio.entity->tio.HATCH != nullptr) {
        convertHatch(object->tio.entity->tio.HATCH, contours);
      }
      const std::string layer = layerNameOf(&dwg, object);
      const uint32_t color = colorOf(object->tio.entity->color);
      for (Entity& contour : contours) {
        contour.id = static_cast<uint64_t>(object->handle.value);
        contour.layer = layer;
        contour.color = color;
        if (inModelSpace) {
          scene.entities.push_back(std::move(contour));
        } else {
          scene.blocks[ownerBlock].entities.push_back(std::move(contour));
        }
      }
      continue;
    }

    Entity entity;
    switch (object->fixedtype) {
      case DWG_TYPE_LINE:
        entity = convertLine(object->tio.entity->tio.LINE);
        break;
      case DWG_TYPE_ARC:
        entity = convertArc(object->tio.entity->tio.ARC);
        break;
      case DWG_TYPE_CIRCLE:
        entity = convertCircle(object->tio.entity->tio.CIRCLE);
        break;
      case DWG_TYPE_LWPOLYLINE:
        entity = convertLwPolyline(object->tio.entity->tio.LWPOLYLINE);
        break;
      case DWG_TYPE_POLYLINE_3D: {
        const auto vertices =
            vertexOwners.find(static_cast<unsigned long>(object->handle.value));
        if (vertices == vertexOwners.end()) continue;
        if (object->tio.entity->tio.POLYLINE_3D == nullptr) continue;
        entity = dwgcore::makePolyline(
            vertices->second,
            (object->tio.entity->tio.POLYLINE_3D->flag & 1) != 0);
        break;
      }
      case DWG_TYPE_TRACE:
        entity = convertTrace(object->tio.entity->tio.TRACE);
        break;
      case DWG_TYPE__3DFACE:
        entity = convert3dFace(object->tio.entity->tio._3DFACE);
        break;
      case DWG_TYPE_LEADER:
        entity = convertLeader(object->tio.entity->tio.LEADER);
        break;
      case DWG_TYPE_POLYLINE_2D: {
        const auto vertices =
            vertexOwners.find(static_cast<unsigned long>(object->handle.value));
        if (vertices == vertexOwners.end()) continue;
        entity = convertPolyline2D(object->tio.entity->tio.POLYLINE_2D,
                                   vertices->second);
        break;
      }
      case DWG_TYPE_ELLIPSE:
        entity = convertEllipse(object->tio.entity->tio.ELLIPSE, kMaxSagitta);
        break;
      case DWG_TYPE_SPLINE:
        entity = convertSpline(object->tio.entity->tio.SPLINE, kMaxSagitta);
        break;
      case DWG_TYPE_POINT:
        entity = convertPoint(object->tio.entity->tio.POINT);
        break;
      case DWG_TYPE_SOLID:
        entity = convertSolid(object->tio.entity->tio.SOLID);
        break;
      case DWG_TYPE_TEXT:
        entity = convertText(object->tio.entity->tio.TEXT);
        break;
      case DWG_TYPE_MTEXT:
        entity = convertMText(object->tio.entity->tio.MTEXT);
        break;
      default:
        if (!isStructuralMarker(object->fixedtype)) {
          scene.unsupported[object->name != nullptr ? object->name : "?"] += 1;
        }
        continue;
    }

    entity.id = static_cast<uint64_t>(object->handle.value);
    entity.layer = layerNameOf(&dwg, object);
    entity.color = colorOf(object->tio.entity->color);

    if (inModelSpace) {
      scene.entities.push_back(std::move(entity));
    } else {
      scene.blocks[ownerBlock].entities.push_back(std::move(entity));
    }
  }

  scene.ok = true;
  dwg_free(&dwg);
  return scene;
}

namespace {

// Los bloques de una referencia externa se guardan con el nombre de la xref por
// delante. Dos planos distintos pueden tener ambos un bloque "PUERTA" con
// contenido diferente, y sin separarlos uno pisaría al otro.
std::string namespacedBlock(const std::string& xrefName, const std::string& block) {
  return xrefName + "|" + block;
}

void renameInserts(std::vector<dwgcore::InsertRef>& inserts,
                   const std::string& xrefName) {
  for (dwgcore::InsertRef& insert : inserts) {
    insert.blockName = namespacedBlock(xrefName, insert.blockName);
  }
}

// Incorpora un archivo externo como definición del bloque `xrefName`.
bool mergeOne(ExtractedScene& target, const std::string& xrefName,
              const std::string& path,
              const std::map<std::string, std::string>& resolvedPaths, int depth);

void mergeInto(ExtractedScene& target, ExtractedScene& source,
               const std::string& xrefName) {
  // Los bloques propios del archivo externo entran con nombre separado, y las
  // inserciones que los usan se reescriben para apuntar al nuevo nombre.
  for (auto& [name, definition] : source.blocks) {
    dwgcore::BlockDefinition copy = definition;
    copy.name = namespacedBlock(xrefName, name);
    renameInserts(copy.inserts, xrefName);
    target.blocks[copy.name] = std::move(copy);
  }

  // El espacio modelo del archivo externo pasa a ser el contenido del bloque
  // con el que se inserta en el plano principal.
  dwgcore::BlockDefinition definition;
  definition.name = xrefName;
  definition.entities = std::move(source.entities);
  definition.inserts = std::move(source.inserts);
  renameInserts(definition.inserts, xrefName);
  target.blocks[xrefName] = std::move(definition);

  // Las capas del archivo externo se suman a la tabla sin pisar las del plano
  // principal, que es quien manda cuando coinciden los nombres.
  for (const auto& [name, style] : source.layers) {
    target.layers.emplace(name, style);
  }

  for (const auto& [type, count] : source.unsupported) {
    target.unsupported[type] += count;
  }
}

bool mergeOne(ExtractedScene& target, const std::string& xrefName,
              const std::string& path,
              const std::map<std::string, std::string>& resolvedPaths, int depth) {
  ExtractedScene source = extractScene(path);
  if (!source.ok) return false;

  // Una xref puede contener otras xrefs. Se resuelven contra el mismo mapa,
  // acotando la profundidad para que una cadena circular no cuelgue la carga.
  if (depth > 0) {
    for (const XrefDeclaration& nested : source.xrefs) {
      const auto found = resolvedPaths.find(nested.blockName);
      if (found == resolvedPaths.end()) continue;
      mergeOne(source, nested.blockName, found->second, resolvedPaths, depth - 1);
    }
  }

  mergeInto(target, source, xrefName);
  return true;
}

}  // namespace

XrefResolution mergeXrefs(ExtractedScene& scene,
                          const std::map<std::string, std::string>& resolvedPaths,
                          int maxDepth) {
  XrefResolution resolution;
  if (!scene.ok) return resolution;

  for (const XrefDeclaration& xref : scene.xrefs) {
    const auto found = resolvedPaths.find(xref.blockName);
    if (found == resolvedPaths.end() || found->second.empty()) {
      resolution.unresolvedBlocks.push_back(xref.blockName);
      continue;
    }

    if (mergeOne(scene, xref.blockName, found->second, resolvedPaths, maxDepth - 1)) {
      resolution.resolved += 1;
    } else {
      // El archivo estaba, pero no se pudo leer: distinto de no encontrarlo, y
      // conviene que la interfaz lo diga de otra manera.
      resolution.failed += 1;
      resolution.unresolvedBlocks.push_back(xref.blockName);
    }
  }
  return resolution;
}

}  // namespace dwgapp
