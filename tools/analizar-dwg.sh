#!/usr/bin/env bash
# Informe de Fase 0: qué contiene realmente un DWG según LibreDWG.
#
# Uso:  tools/analizar-dwg.sh <ruta-libredwg-build> <archivo.dwg> [más.dwg...]
#
# Responde a las preguntas que deciden la viabilidad del proyecto: qué versión
# es, cuántas entidades tiene de cada tipo, si trae referencias externas, y
# cuánto tarda LibreDWG en leerlo.
set -uo pipefail

if [ $# -lt 2 ]; then
  echo "Uso: $0 <ruta-build-libredwg> <archivo.dwg> [más.dwg...]" >&2
  exit 1
fi

BUILD_DIR="$1"
shift

DWGREAD="$BUILD_DIR/dwgread"
DWG2DXF="$BUILD_DIR/dwg2dxf"
export LD_LIBRARY_PATH="$BUILD_DIR:${LD_LIBRARY_PATH:-}"

for tool in "$DWGREAD" "$DWG2DXF"; do
  if [ ! -x "$tool" ]; then
    echo "No encuentro $tool. ¿Está compilado LibreDWG en $BUILD_DIR?" >&2
    exit 1
  fi
done

# Tipos que importan para dibujar y medir. HATCH y SPLINE se listan aparte
# porque son los que deciden el peso del render y la precisión de la medición.
TIPOS=(LINE LWPOLYLINE POLYLINE ARC CIRCLE ELLIPSE SPLINE INSERT TEXT MTEXT
       DIMENSION HATCH SOLID POINT LEADER)

# `stat -c%s` es de GNU; en macOS la opción equivalente es `-f%z`.
tamano_bytes() {
  stat -f%z "$1" 2>/dev/null || stat -c%s "$1" 2>/dev/null || echo 0
}

# `date +%s.%N` tampoco es portable: el date de macOS no conoce %N y devuelve la
# letra literal, que luego rompe la resta. perl siempre está (LibreDWG lo exige).
ahora() {
  perl -MTime::HiRes=time -e 'printf "%.6f\n", time' 2>/dev/null || date +%s
}

for dwg in "$@"; do
  if [ ! -f "$dwg" ]; then
    echo "No existe: $dwg" >&2
    continue
  fi

  echo "════════════════════════════════════════════════════════════"
  echo "ARCHIVO: $(basename "$dwg")  ($(( $(tamano_bytes "$dwg") / 1024 )) KB)"
  echo "════════════════════════════════════════════════════════════"

  # La versión solo se imprime a partir de -v2.
  version=$("$DWGREAD" -v2 "$dwg" 2>&1 | sed -n 's/.*version code is: //p' | head -1)
  echo "Versión DWG      : ${version:-desconocida}"

  # dwg2dxf se niega a escribir si el destino ya existe, así que se reserva el
  # nombre sin crear el fichero.
  # `mktemp -u --suffix=` es de GNU; el mktemp de macOS no lo entiende. Como aquí
  # solo hace falta un nombre libre, se compone a mano.
  dxf="${TMPDIR:-/tmp}/analizar-dwg-$$-${RANDOM}.dxf"
  inicio=$(ahora)
  "$DWG2DXF" -o "$dxf" "$dwg" >/dev/null 2>&1
  salida=$?
  fin=$(ahora)
  printf "Conversión a DXF : código %s en %.2f s\n" "$salida" "$(echo "$fin - $inicio" | bc)"

  if [ ! -s "$dxf" ]; then
    echo "⚠️  No se generó DXF. LibreDWG no puede leer este archivo."
    rm -f "$dxf"
    continue
  fi

  # El DXF sale con finales de línea CRLF: hay que quitarlos antes de contar.
  plano=$(mktemp)
  tr -d '\r' < "$dxf" > "$plano"

  echo
  echo "Entidades por tipo:"
  total=0
  for tipo in "${TIPOS[@]}"; do
    n=$(grep -cx "$tipo" "$plano" || true)
    if [ "$n" -gt 0 ]; then
      printf "  %-12s %8d\n" "$tipo" "$n"
      total=$((total + n))
    fi
  done
  printf "  %-12s %8d\n" "TOTAL" "$total"

  capas=$(awk '/^TABLE$/{t=1} t&&/^LAYER$/{n++} /^ENDTAB$/{t=0} END{print n+0}' "$plano")
  bloques=$(grep -cx "BLOCK" "$plano" || true)
  echo
  echo "Capas            : $capas"
  echo "Bloques          : $bloques"

  # Las referencias externas aparecen como bloques con la ruta al archivo
  # externo en el código de grupo 1 dentro del registro de bloque.
  echo
  echo "Referencias externas (xrefs):"
  xrefs=$(grep -iE "\.dwg$" "$plano" | sort -u || true)
  if [ -z "$xrefs" ]; then
    echo "  ninguna detectada"
  else
    echo "$xrefs" | sed 's/^/  /'
  fi

  rm -f "$dxf" "$plano"
  echo
done
