# Versión de LibreDWG con la que se compila la app. La comparten el script de
# Android y el de escritorio.
#
# Se fija a un commit concreto a propósito: LibreDWG se desarrolla sobre su rama
# principal y cambia nombres de campos entre versiones, así que clonar "lo
# último" hace que un proyecto que compilaba ayer deje de compilar hoy sin que
# nadie haya tocado nada. Para actualizar, cambia el commit, recompila y pasa
# los tests.
LIBREDWG_COMMIT="bb1e8af298ef64388431955243a88a299112fd90"
LIBREDWG_URL="https://github.com/LibreDWG/libredwg.git"

# Deja en "$1" el código de LibreDWG en el commit fijado, con su submódulo.
obtener_libredwg() {
  destino="$1"
  if [ ! -d "$destino/.git" ]; then
    echo "▶ Descargando LibreDWG ($LIBREDWG_COMMIT)…" >&2
    mkdir -p "$destino"
    git -C "$destino" init -q
    git -C "$destino" remote add origin "$LIBREDWG_URL"
  fi
  if [ "$(git -C "$destino" rev-parse HEAD 2>/dev/null || true)" != "$LIBREDWG_COMMIT" ]; then
    git -C "$destino" fetch -q --depth 1 origin "$LIBREDWG_COMMIT"
    git -C "$destino" checkout -q FETCH_HEAD
  fi
  # El submódulo jsmn DEBE estar aunque se desactive el soporte JSON: sin él el
  # build muere en in_json.c con un error que no menciona ningún submódulo.
  git -C "$destino" submodule update --init --depth 1 >&2
}
