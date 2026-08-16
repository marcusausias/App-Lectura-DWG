// Puente JNI. Mantiene el mínimo de lógica posible: solo traduce entre tipos
// de Java y las estructuras de dwg_document.h.

#include <jni.h>

#include <string>

#include "dwg_document.h"

namespace {

jstring toJava(JNIEnv* env, const std::string& text) {
  return env->NewStringUTF(text.c_str());
}

// Construye el DocumentSummary de Kotlin a partir del de C++.
jobject buildSummary(JNIEnv* env, const dwgapp::DocumentSummary& summary) {
  jclass summaryClass =
      env->FindClass("com/marcusausias/dwgviewer/nativebridge/DocumentSummary");
  if (summaryClass == nullptr) return nullptr;

  jclass xrefClass = env->FindClass("com/marcusausias/dwgviewer/nativebridge/XrefRef");
  if (xrefClass == nullptr) return nullptr;

  jmethodID xrefInit = env->GetMethodID(
      xrefClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Z)V");
  jobjectArray xrefs = env->NewObjectArray(
      static_cast<jsize>(summary.xrefs.size()), xrefClass, nullptr);
  for (size_t i = 0; i < summary.xrefs.size(); ++i) {
    const dwgapp::XrefRef& xref = summary.xrefs[i];
    jstring name = toJava(env, xref.name);
    jstring path = toJava(env, xref.path);
    jobject item = env->NewObject(xrefClass, xrefInit, name, path,
                                  static_cast<jboolean>(xref.isOverlay));
    env->SetObjectArrayElement(xrefs, static_cast<jsize>(i), item);
    env->DeleteLocalRef(name);
    env->DeleteLocalRef(path);
    env->DeleteLocalRef(item);
  }

  jmethodID summaryInit =
      env->GetMethodID(summaryClass, "<init>",
                       "(ZLjava/lang/String;Ljava/lang/String;IJJJJ"
                       "[Lcom/marcusausias/dwgviewer/nativebridge/XrefRef;"
                       "Ljava/lang/String;)V");
  if (summaryInit == nullptr) return nullptr;

  return env->NewObject(
      summaryClass, summaryInit, static_cast<jboolean>(summary.opened),
      toJava(env, summary.versionCode), toJava(env, summary.versionName),
      static_cast<jint>(summary.libredwgError),
      static_cast<jlong>(summary.objectCount),
      static_cast<jlong>(summary.entityCount),
      static_cast<jlong>(summary.layerCount),
      static_cast<jlong>(summary.blockCount), xrefs,
      toJava(env, summary.errorMessage));
}

}  // namespace

extern "C" JNIEXPORT jobject JNICALL
Java_com_marcusausias_dwgviewer_nativebridge_DwgNative_summarize(JNIEnv* env, jobject,
                                                           jstring path) {
  const char* rawPath = env->GetStringUTFChars(path, nullptr);
  if (rawPath == nullptr) return nullptr;

  const dwgapp::DocumentSummary summary = dwgapp::summarize(std::string(rawPath));
  env->ReleaseStringUTFChars(path, rawPath);

  return buildSummary(env, summary);
}
