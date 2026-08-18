// Puente JNI de la escena: mantiene un plano abierto entre llamadas.
//
// La escena vive en C++ y Kotlin solo guarda un handle. Los vértices se
// entregan como ByteBuffer directo, sin copiarlos al montón de Java: son
// varios megas por plano y copiarlos sería tirar memoria y tiempo.

#include <jni.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "dwg_extract.h"
#include "dwgcore/cache.h"
#include "dwgcore/measure.h"
#include "dwgcore/snap.h"
#include "dwgcore/render.h"
#include "dwgcore/scene.h"

namespace {

struct OpenPlan {
  dwgcore::Scene scene;
  dwgcore::RenderData render;
};

// El handle que viaja a Kotlin es el puntero. Mientras exista, el ByteBuffer
// que se le ha entregado sigue apuntando a memoria válida; por eso cerrarlo
// antes de tiempo deja la vista dibujando sobre memoria liberada.
OpenPlan* fromHandle(jlong handle) {
  return reinterpret_cast<OpenPlan*>(handle);
}

// Lista de capas ocultas. Se copia porque el array de Java puede moverse en
// cuanto se ejecute el recolector de basura.
std::vector<uint16_t> toLayerList(JNIEnv* env, jintArray layers) {
  std::vector<uint16_t> result;
  if (layers == nullptr) return result;

  const jsize count = env->GetArrayLength(layers);
  std::vector<jint> raw(static_cast<size_t>(count));
  env->GetIntArrayRegion(layers, 0, count, raw.data());
  result.reserve(raw.size());
  for (jint value : raw) result.push_back(static_cast<uint16_t>(value));
  return result;
}

std::string toString(JNIEnv* env, jstring text) {
  if (text == nullptr) return {};
  const char* raw = env->GetStringUTFChars(text, nullptr);
  if (raw == nullptr) return {};
  std::string result(raw);
  env->ReleaseStringUTFChars(text, raw);
  return result;
}

}  // namespace

extern "C" {

JNIEXPORT jlong JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_openPlan(
    JNIEnv* env, jobject, jstring dwgPath, jstring cachePath, jlong sourceSize,
    jlong sourceModified, jlong variant, jobjectArray xrefNames,
    jobjectArray xrefPaths) {
  const std::string dwg = toString(env, dwgPath);
  const std::string cache = toString(env, cachePath);

  dwgcore::CacheStamp stamp;
  stamp.sourceSize = static_cast<uint64_t>(sourceSize);
  stamp.sourceModified = static_cast<uint64_t>(sourceModified);
  stamp.variant = static_cast<uint64_t>(variant);

  auto plan = std::make_unique<OpenPlan>();

  // Camino rápido: si hay caché válida no se vuelve a tocar el DWG.
  if (!dwgcore::readCache(cache, stamp, plan->scene)) {
    dwgapp::ExtractedScene extracted = dwgapp::extractScene(dwg);
    if (!extracted.ok) return 0;

    // Referencias externas ya localizadas por la capa de Android, que es la
    // única que sabe traducir una ruta de Windows a un archivo del dispositivo.
    std::map<std::string, std::string> resolved;
    if (xrefNames != nullptr && xrefPaths != nullptr) {
      const jsize count =
          std::min(env->GetArrayLength(xrefNames), env->GetArrayLength(xrefPaths));
      for (jsize i = 0; i < count; ++i) {
        auto name = static_cast<jstring>(env->GetObjectArrayElement(xrefNames, i));
        auto path = static_cast<jstring>(env->GetObjectArrayElement(xrefPaths, i));
        resolved[toString(env, name)] = toString(env, path);
        env->DeleteLocalRef(name);
        env->DeleteLocalRef(path);
      }
    }
    dwgapp::mergeXrefs(extracted, resolved);

    dwgcore::FlattenResult flat =
        dwgcore::flatten(extracted.entities, extracted.inserts, extracted.blocks);
    plan->scene = dwgcore::buildScene(std::move(flat.entities));

    // Que no se pueda escribir la caché no impide usar el plano: solo hará que
    // la próxima apertura vuelva a tardar.
    dwgcore::writeCache(cache, plan->scene, stamp);
  }

  plan->render = dwgcore::buildRenderData(plan->scene);
  return reinterpret_cast<jlong>(plan.release());
}

JNIEXPORT void JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_closePlan(JNIEnv*, jobject,
                                                                  jlong handle) {
  delete fromHandle(handle);
}

// Buffer de vértices sin copia. Su contenido no se modifica nunca después de
// abrir el plano, así que puede compartirse con la GPU sin sincronizar nada.
JNIEXPORT jobject JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_getVertexBuffer(
    JNIEnv* env, jobject, jlong handle) {
  OpenPlan* plan = fromHandle(handle);
  if (plan == nullptr || plan->render.vertices.empty()) return nullptr;
  return env->NewDirectByteBuffer(
      plan->render.vertices.data(),
      static_cast<jlong>(plan->render.vertices.size() * sizeof(float)));
}

// Lotes aplanados de tres en tres: capa, primer vértice, número de vértices.
JNIEXPORT jintArray JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_getBatches(JNIEnv* env,
                                                                   jobject,
                                                                   jlong handle) {
  OpenPlan* plan = fromHandle(handle);
  if (plan == nullptr) return nullptr;

  const std::vector<dwgcore::RenderBatch>& batches = plan->render.batches;
  std::vector<jint> flat;
  flat.reserve(batches.size() * 3);
  for (const dwgcore::RenderBatch& batch : batches) {
    flat.push_back(static_cast<jint>(batch.layerId));
    flat.push_back(static_cast<jint>(batch.firstVertex));
    flat.push_back(static_cast<jint>(batch.vertexCount));
  }

  jintArray result = env->NewIntArray(static_cast<jsize>(flat.size()));
  if (result == nullptr) return nullptr;
  env->SetIntArrayRegion(result, 0, static_cast<jsize>(flat.size()), flat.data());
  return result;
}

JNIEXPORT jobjectArray JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_getLayers(JNIEnv* env,
                                                                  jobject,
                                                                  jlong handle) {
  OpenPlan* plan = fromHandle(handle);
  if (plan == nullptr) return nullptr;

  jclass stringClass = env->FindClass("java/lang/String");
  jobjectArray result = env->NewObjectArray(
      static_cast<jsize>(plan->scene.layers.size()), stringClass, nullptr);
  for (size_t i = 0; i < plan->scene.layers.size(); ++i) {
    jstring name = env->NewStringUTF(plan->scene.layers[i].c_str());
    env->SetObjectArrayElement(result, static_cast<jsize>(i), name);
    env->DeleteLocalRef(name);
  }
  return result;
}

// Extensión del plano y origen restado a los vértices, en este orden:
// minX, minY, maxX, maxY, originX, originY.
JNIEXPORT jdoubleArray JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_getBounds(JNIEnv* env,
                                                                  jobject,
                                                                  jlong handle) {
  OpenPlan* plan = fromHandle(handle);
  if (plan == nullptr) return nullptr;

  const dwgcore::Bounds& bounds = plan->scene.bounds;
  const jdouble values[6] = {bounds.min.x,           bounds.min.y,
                             bounds.max.x,           bounds.max.y,
                             plan->render.origin.x,  plan->render.origin.y};
  jdoubleArray result = env->NewDoubleArray(6);
  if (result == nullptr) return nullptr;
  env->SetDoubleArrayRegion(result, 0, 6, values);
  return result;
}

JNIEXPORT jint JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_getEntityCount(
    JNIEnv*, jobject, jlong handle) {
  OpenPlan* plan = fromHandle(handle);
  return plan == nullptr ? 0 : static_cast<jint>(plan->scene.entities.size());
}

// Textos visibles en el área dada y con altura suficiente para leerse.
//
// El filtro por altura no es un adorno: un plano de instalaciones tiene miles
// de textos, y a poco zoom todos ellos son manchas ilegibles que además hay que
// medir y dibujar. Descartarlos aquí es lo que mantiene el paneo fluido.
JNIEXPORT jobjectArray JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_getVisibleTexts(
    JNIEnv* env, jobject, jlong handle, jdouble minX, jdouble minY, jdouble maxX,
    jdouble maxY, jdouble minHeight, jint limit) {
  OpenPlan* plan = fromHandle(handle);
  if (plan == nullptr) return nullptr;

  dwgcore::Bounds area;
  area.expand(dwgcore::Vec2{minX, minY});
  area.expand(dwgcore::Vec2{maxX, maxY});

  std::vector<uint32_t> candidates;
  dwgcore::queryEntities(plan->scene, area, candidates);

  jclass textClass =
      env->FindClass("com/marcusausias/dwgviewer/nativebridge/PlanText");
  if (textClass == nullptr) return nullptr;
  jmethodID init =
      env->GetMethodID(textClass, "<init>", "(Ljava/lang/String;DDDDI)V");
  if (init == nullptr) return nullptr;

  std::vector<jobject> found;
  for (uint32_t index : candidates) {
    if (static_cast<jint>(found.size()) >= limit) break;

    const dwgcore::Entity& entity = plan->scene.entities[index];
    if (entity.type != dwgcore::EntityType::Text) continue;
    if (entity.text.empty() || entity.vertices.empty()) continue;
    if (entity.textHeight < minHeight) continue;

    jstring content = env->NewStringUTF(entity.text.c_str());
    jobject item = env->NewObject(
        textClass, init, content, entity.vertices[0].position.x,
        entity.vertices[0].position.y, entity.textHeight, entity.textRotation,
        static_cast<jint>(entity.layerId));
    env->DeleteLocalRef(content);
    found.push_back(item);
  }

  jobjectArray result =
      env->NewObjectArray(static_cast<jsize>(found.size()), textClass, nullptr);
  for (size_t i = 0; i < found.size(); ++i) {
    env->SetObjectArrayElement(result, static_cast<jsize>(i), found[i]);
    env->DeleteLocalRef(found[i]);
  }
  return result;
}


// Referencias externas que declara un DWG, sin llegar a abrirlo del todo.
//
// Se consulta antes de procesar el plano: la capa de Android necesita saber qué
// archivos buscar en la carpeta del proyecto, y con qué nombre de bloque
// asociarlos.
//
// Devuelve pares (nombre de bloque, ruta original) aplanados.
JNIEXPORT jobjectArray JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_inspectXrefs(
    JNIEnv* env, jobject, jstring dwgPath) {
  const dwgapp::ExtractedScene scene = dwgapp::extractScene(toString(env, dwgPath));

  jclass stringClass = env->FindClass("java/lang/String");
  jobjectArray result = env->NewObjectArray(
      static_cast<jsize>(scene.xrefs.size() * 2), stringClass, nullptr);

  for (size_t i = 0; i < scene.xrefs.size(); ++i) {
    jstring name = env->NewStringUTF(scene.xrefs[i].blockName.c_str());
    jstring path = env->NewStringUTF(scene.xrefs[i].rawPath.c_str());
    env->SetObjectArrayElement(result, static_cast<jsize>(i * 2), name);
    env->SetObjectArrayElement(result, static_cast<jsize>(i * 2 + 1), path);
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(path);
  }
  return result;
}

// Enganche del dedo al punto notable más cercano.
//
// `radius` llega ya en unidades de dibujo: quien llama lo obtiene de un radio
// en píxeles, porque lo que tiene que sentirse constante es la distancia en
// pantalla, no en el dibujo.
//
// Devuelve {tipo, x, y, índiceDeEntidad} o null si no hay nada que enganchar.
JNIEXPORT jdoubleArray JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_snapAt(
    JNIEnv* env, jobject, jlong handle, jdouble x, jdouble y, jdouble radius,
    jdouble referenceX, jdouble referenceY, jboolean hasReference,
    jintArray hiddenLayers) {
  OpenPlan* plan = fromHandle(handle);
  if (plan == nullptr) return nullptr;

  dwgcore::SnapOptions options;
  options.radius = radius;
  options.hasReference = hasReference == JNI_TRUE;
  options.reference = {referenceX, referenceY};

  const dwgcore::SnapResult result =
      dwgcore::snap(plan->scene, {x, y}, options, toLayerList(env, hiddenLayers));
  if (!result.found()) return nullptr;

  const jdouble values[4] = {static_cast<jdouble>(result.type), result.point.x,
                             result.point.y,
                             static_cast<jdouble>(result.entityIndex)};
  jdoubleArray out = env->NewDoubleArray(4);
  if (out == nullptr) return nullptr;
  env->SetDoubleArrayRegion(out, 0, 4, values);
  return out;
}

// Entidad tocada, o -1 si no hay ninguna dentro del radio.
JNIEXPORT jint JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_pickEntity(
    JNIEnv* env, jobject, jlong handle, jdouble x, jdouble y, jdouble radius,
    jintArray hiddenLayers) {
  OpenPlan* plan = fromHandle(handle);
  if (plan == nullptr) return -1;

  const uint32_t index = dwgcore::pickEntity(plan->scene, {x, y}, radius,
                                             toLayerList(env, hiddenLayers));
  return index == dwgcore::kNoEntity ? -1 : static_cast<jint>(index);
}

// Longitud de una entidad siguiéndola entera, con sus arcos exactos.
// Devuelve {longitud, esAproximada, nºPuntos, x0, y0, x1, y1, ...}.
JNIEXPORT jdoubleArray JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_measureEntity(
    JNIEnv* env, jobject, jlong handle, jint entityIndex) {
  OpenPlan* plan = fromHandle(handle);
  if (plan == nullptr || entityIndex < 0) return nullptr;

  const dwgcore::Measurement measurement =
      dwgcore::measureEntity(plan->scene, static_cast<uint32_t>(entityIndex));

  // Un tope de puntos: una spline muy teselada no necesita viajar entera solo
  // para dibujar el resaltado de lo que se ha medido.
  const size_t maxPoints = 2000;
  const size_t pointCount = std::min(measurement.points.size(), maxPoints);

  std::vector<jdouble> values;
  values.reserve(3 + pointCount * 2);
  values.push_back(measurement.value);
  values.push_back(measurement.approximate ? 1.0 : 0.0);
  values.push_back(static_cast<jdouble>(pointCount));
  for (size_t i = 0; i < pointCount; ++i) {
    values.push_back(measurement.points[i].x);
    values.push_back(measurement.points[i].y);
  }

  jdoubleArray out = env->NewDoubleArray(static_cast<jsize>(values.size()));
  if (out == nullptr) return nullptr;
  env->SetDoubleArrayRegion(out, 0, static_cast<jsize>(values.size()), values.data());
  return out;
}

// Cuántos símbolos iguales al de esta entidad hay en el plano.
// Devuelve el nombre del símbolo, o null si la entidad no viene de ninguno.
JNIEXPORT jstring JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_countSymbolName(
    JNIEnv* env, jobject, jlong handle, jint entityIndex) {
  OpenPlan* plan = fromHandle(handle);
  if (plan == nullptr || entityIndex < 0) return nullptr;

  const dwgcore::Measurement measurement =
      dwgcore::measureCount(plan->scene, static_cast<uint32_t>(entityIndex));
  if (measurement.count == 0) return nullptr;
  return env->NewStringUTF(measurement.label.c_str());
}

JNIEXPORT jint JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_PlanNative_countSymbolInstances(
    JNIEnv*, jobject, jlong handle, jint entityIndex) {
  OpenPlan* plan = fromHandle(handle);
  if (plan == nullptr || entityIndex < 0) return 0;
  return dwgcore::measureCount(plan->scene, static_cast<uint32_t>(entityIndex)).count;
}

}  // extern "C"
