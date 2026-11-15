// Pasa un DWG por la misma cadena que la app y enseña qué sale.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <map>
#include <string>
#include <vector>

#include "dwg_extract.h"
#include "dwgcore/flatten.h"
#include "dwgcore/render.h"
#include "dwgcore/scene.h"

namespace {

struct Image {
  int width, height;
  std::vector<unsigned char> pixels;
  Image(int w, int h) : width(w), height(h), pixels(size_t(w) * h * 3, 239) {}

  void plot(int x, int y, const unsigned char* rgb) {
    if (x < 0 || y < 0 || x >= width || y >= height) return;
    unsigned char* p = &pixels[(size_t(y) * width + x) * 3];
    p[0] = rgb[0]; p[1] = rgb[1]; p[2] = rgb[2];
  }

  void line(double x0, double y0, double x1, double y1, const unsigned char* rgb) {
    const double dx = x1 - x0, dy = y1 - y0;
    const int steps = std::clamp(int(std::max(std::fabs(dx), std::fabs(dy))) + 1, 1, 8 * (width + height));
    for (int i = 0; i <= steps; ++i) {
      const double t = double(i) / steps;
      plot(int(std::lround(x0 + dx * t)), int(std::lround(y0 + dy * t)), rgb);
    }
  }
};

}  // namespace

int main(int argc, char** argv) {
  if (argc < 2) {
    std::fprintf(stderr, "uso: dwgprobe plano.dwg [salida.ppm] [ancho]\n");
    return 2;
  }
  const auto t0 = std::chrono::steady_clock::now();

  dwgapp::ExtractedScene extracted = dwgapp::extractScene(argv[1]);
  if (!extracted.ok) {
    std::fprintf(stderr, "ERROR: %s\n", extracted.errorMessage.c_str());
    return 1;
  }
  dwgapp::mergeXrefs(extracted, {});

  const size_t looseEntities = extracted.entities.size();
  const size_t looseInserts = extracted.inserts.size();
  const size_t blockCount = extracted.blocks.size();

  dwgcore::FlattenResult flat =
      dwgcore::flatten(extracted.entities, extracted.inserts, extracted.blocks);
  dwgcore::Scene scene = dwgcore::buildScene(std::move(flat.entities), 16, &extracted.layers);
  dwgcore::RenderData render = dwgcore::buildRenderData(scene);

  const auto t1 = std::chrono::steady_clock::now();
  const double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();

  std::printf("archivo            %s\n", argv[1]);
  std::printf("tiempo             %.0f ms\n", ms);
  std::printf("espacio modelo     %zu entidades sueltas, %zu inserciones\n", looseEntities, looseInserts);
  std::printf("bloques            %zu\n", blockCount);
  std::printf("xrefs declaradas   %zu\n", extracted.xrefs.size());
  std::printf("escena             %zu entidades, %zu capas, %zu segmentos\n",
              scene.entities.size(), scene.layers.size(), render.segmentCount());
  if (scene.bounds.valid) {
    std::printf("extensión          (%.3f, %.3f) - (%.3f, %.3f)\n", scene.bounds.min.x,
                scene.bounds.min.y, scene.bounds.max.x, scene.bounds.max.y);
  }
  size_t hiddenLayers = 0;
  for (const dwgcore::LayerStyle& style : scene.layerStyles) hiddenLayers += style.hidden;
  std::printf("capas ocultas      %zu\n", hiddenLayers);
  size_t texts = 0;
  for (const dwgcore::Entity& e : scene.entities) texts += e.type == dwgcore::EntityType::Text;
  std::printf("textos             %zu\n", texts);
  for (const auto& [type, count] : extracted.unsupported) {
    std::printf("sin convertir      %-24s %d\n", type.c_str(), count);
  }

  if (argc < 3 || !scene.bounds.valid) return 0;

  const int width = argc > 3 ? std::atoi(argv[3]) : 1600;
  const double w = scene.bounds.max.x - scene.bounds.min.x;
  const double h = scene.bounds.max.y - scene.bounds.min.y;
  if (!(w > 0.0) || !(h > 0.0)) return 0;
  const int height = std::clamp(int(width * h / w), 1, 8000);
  const double scale = std::min((width - 20) / w, (height - 20) / h);

  Image image(width, height);
  const double cx = (scene.bounds.min.x + scene.bounds.max.x) * 0.5;
  const double cy = (scene.bounds.min.y + scene.bounds.max.y) * 0.5;

  for (const dwgcore::RenderBatch& batch : render.batches) {
    unsigned char rgb[3] = {26, 28, 32};
    if (batch.color >= 0xFF000000u) {
      rgb[0] = (batch.color >> 16) & 255;
      rgb[1] = (batch.color >> 8) & 255;
      rgb[2] = batch.color & 255;
    }
    if (batch.layerId < scene.layerStyles.size() &&
        scene.layerStyles[batch.layerId].hidden) {
      continue;
    }
    for (uint32_t v = batch.firstVertex; v + 1 < batch.firstVertex + batch.vertexCount; v += 2) {
      const double x0 = render.vertices[v * 2] + render.origin.x;
      const double y0 = render.vertices[v * 2 + 1] + render.origin.y;
      const double x1 = render.vertices[v * 2 + 2] + render.origin.x;
      const double y1 = render.vertices[v * 2 + 3] + render.origin.y;
      image.line((x0 - cx) * scale + width / 2.0, (cy - y0) * scale + height / 2.0,
                 (x1 - cx) * scale + width / 2.0, (cy - y1) * scale + height / 2.0, rgb);
    }
  }

  std::ofstream out(argv[2], std::ios::binary);
  out << "P6\n" << width << " " << height << "\n255\n";
  out.write(reinterpret_cast<const char*>(image.pixels.data()),
            static_cast<std::streamsize>(image.pixels.size()));
  std::printf("imagen             %s (%dx%d)\n", argv[2], width, height);
  return 0;
}
