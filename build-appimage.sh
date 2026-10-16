#!/usr/bin/env bash
# =============================================================================
#  build-appimage.sh — Blackcurrant Music Player (Qt/C++)
#
#  Membuat AppImage yang benar-benar standalone memakai linuxdeploy:
#    * Qt 6 + plugin Multimedia (FFmpeg)  -> linuxdeploy + linuxdeploy-plugin-qt
#    * Python portable + ytmusicapi/requests/Pillow (untuk bridge/bridge.py)
#    * yt-dlp (binary mandiri) untuk mengambil URL stream audio
#
#  Nama, versi, developer, dan maintainer dibaca dari src/version.h — sumber yang
#  sama dengan halaman About — lalu dipakai untuk .desktop dan metainfo AppImage.
#
#  Pemakaian:
#    ./build-appimage.sh                  # build penuh
#    ./build-appimage.sh --install-qt     # unduh Qt dulu lewat aqtinstall
#    QT_DIR=$HOME/Qt/6.7.3/gcc_64 ./build-appimage.sh
#    ./build-appimage.sh --clean          # hapus folder kerja lalu build
#    ./build-appimage.sh --help
#
#  Hasil: dist/Blackcurrant_Music_Player-<versi>-<arch>.AppImage
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
# Bila skrip ini dijalankan dari salinan di luar folder proyek, cari proyeknya:
# folder kerja saat ini, atau subfolder music_player_qt di dekat skrip / folder kerja.
if [ ! -f "$ROOT/src/version.h" ]; then
    for _c in "$PWD" "$ROOT/music_player_qt" "$PWD/music_player_qt"; do
        if [ -f "$_c/src/version.h" ]; then ROOT="$_c"; break; fi
    done
fi
WORK="${WORK:-$ROOT/build-appimage}"
APPDIR="$WORK/AppDir"
TOOLS="$WORK/tools"
DIST="${DIST:-$ROOT/dist}"
VERSION_H="$ROOT/src/version.h"

log()  { printf '\n\033[1;35m==>\033[0m \033[1m%s\033[0m\n' "$*"; }
info() { printf '    %s\n' "$*"; }
warn() { printf '\033[1;33mPERINGATAN:\033[0m %s\n' "$*" >&2; }
die()  { printf '\033[1;31mGAGAL:\033[0m %s\n' "$*" >&2; exit 1; }

usage() { sed -n '2,22p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'; exit 0; }

# ---------------------------------------------------------------- argumen
INSTALL_QT=0
CLEAN=0
ONLY=""
while [ $# -gt 0 ]; do
    case "$1" in
        --install-qt) INSTALL_QT=1 ;;
        --clean)      CLEAN=1 ;;
        --only)       ONLY="${2:-}"; shift ;;   # python | ytdlp (untuk uji bagian tertentu)
        -h|--help)    usage ;;
        *) die "Argumen tidak dikenal: $1 (coba --help)" ;;
    esac
    shift
done

# ---------------------------------------------------------------- info aplikasi
[ -f "$VERSION_H" ] || die "src/version.h tidak ditemukan. Jalankan skrip dari dalam folder music_player_qt (atau di folder yang berisi music_player_qt)."
read_const() {
    sed -n "s/.*constexpr const char \*$1 = \"\(.*\)\";.*/\1/p" "$VERSION_H" | head -n1
}
APP_NAME="$(read_const kName)"
APP_VERSION="$(read_const kVersion)"
APP_DEVELOPER="$(read_const kDeveloper)"
APP_MAINTAINER="$(read_const kMaintainer)"
[ -n "$APP_NAME" ] && [ -n "$APP_VERSION" ] && [ -n "$APP_DEVELOPER" ] && [ -n "$APP_MAINTAINER" ] \
    || die "Gagal membaca nama/versi/developer/maintainer dari src/version.h."

# Deskripsi — sama dengan teks di halaman About.
APP_DESC_EN="Open-source desktop music player built with Qt/C++, using YouTube Music as its data source."
APP_DESC_ID="Pemutar musik desktop sumber terbuka berbasis Qt/C++ dengan sumber data YouTube Music."
APP_BIN="blackcurrant"                  # nama target di CMakeLists.txt
APP_ID="${APP_ID:-org.oogr.blackcurrant}"
APP_LICENSE="${BC_LICENSE:-LicenseRef-OpenSource}"   # ganti dengan ID SPDX lisensi sebenarnya
FILE_BASE="$(printf '%s' "$APP_NAME" | tr ' ' '_')"

# ---------------------------------------------------------------- arsitektur
case "$(uname -m)" in
    x86_64)  ARCH=x86_64;  PY_TRIPLE=x86_64-unknown-linux-gnu;  YTDLP_ASSET=yt-dlp_linux ;;
    aarch64) ARCH=aarch64; PY_TRIPLE=aarch64-unknown-linux-gnu; YTDLP_ASSET=yt-dlp_linux_aarch64 ;;
    *) die "Arsitektur $(uname -m) belum didukung." ;;
esac

# Versi yang bisa diatur lewat environment.
PY_SERIES="${PY_SERIES:-3.12}"          # seri Python yang dibundel
QT_VERSION="${QT_VERSION:-6.7.3}"       # dipakai oleh --install-qt
QT_ARCH="${QT_ARCH:-linux_gcc_64}"
QT_MODULES="${QT_MODULES:-qtmultimedia}"

AUTH_HEADER=()
[ -n "${GITHUB_TOKEN:-}" ] && AUTH_HEADER=(-H "Authorization: Bearer $GITHUB_TOKEN")

need() { command -v "$1" >/dev/null 2>&1 || die "Perintah '$1' tidak ditemukan. Pasang dulu."; }

download() { # url tujuan
    info "unduh: $1"
    curl -fL --retry 3 --retry-delay 2 -sS ${AUTH_HEADER[@]+"${AUTH_HEADER[@]}"} -o "$2" "$1" \
        || die "Gagal mengunduh $1"
}

# ================================================================ Qt
install_qt() {
    log "Memasang Qt $QT_VERSION lewat aqtinstall"
    need python3
    [ "$ARCH" = x86_64 ] || die "--install-qt hanya untuk x86_64; pasang Qt manual lalu set QT_DIR."
    python3 -m venv "$WORK/aqt-venv"
    "$WORK/aqt-venv/bin/pip" install -q --upgrade pip aqtinstall
    # shellcheck disable=SC2086
    "$WORK/aqt-venv/bin/aqt" install-qt linux desktop "$QT_VERSION" "$QT_ARCH" \
        -m $QT_MODULES -O "$WORK/Qt"
    QT_DIR="$WORK/Qt/$QT_VERSION/gcc_64"
    [ -x "$QT_DIR/bin/qmake" ] || die "Qt terpasang tetapi $QT_DIR/bin/qmake tidak ada."
}

resolve_qt() {
    if [ -n "${QT_DIR:-}" ]; then
        QMAKE="$QT_DIR/bin/qmake"
        export CMAKE_PREFIX_PATH="$QT_DIR${CMAKE_PREFIX_PATH:+:$CMAKE_PREFIX_PATH}"
        export PATH="$QT_DIR/bin:$PATH"
    else
        QMAKE="$(command -v qmake6 || command -v qmake || true)"
    fi
    [ -n "$QMAKE" ] && [ -x "$QMAKE" ] \
        || die "qmake Qt 6 tidak ditemukan. Set QT_DIR=/path/ke/Qt/6.x/gcc_64 atau pakai --install-qt."
    local ver maj min
    ver="$("$QMAKE" -query QT_VERSION)"
    IFS=. read -r maj min _ <<<"$ver"
    if [ "${maj:-0}" -lt 6 ] || { [ "$maj" -eq 6 ] && [ "${min:-0}" -lt 5 ]; }; then
        die "Qt $ver terlalu lama. Proyek ini butuh Qt 6.5 atau lebih baru (QMediaPlayer::playingChanged)."
    fi
    export QMAKE
    info "Qt $ver  ($QMAKE)"
}

# ================================================================ alat linuxdeploy
fetch_tools() {
    log "Mengunduh linuxdeploy + plugin"
    mkdir -p "$TOOLS"
    local base="https://github.com/linuxdeploy"
    local -a files=(
        "linuxdeploy/releases/download/continuous/linuxdeploy-$ARCH.AppImage"
        "linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-$ARCH.AppImage"
        "linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-$ARCH.AppImage"
    )
    local f out
    for f in "${files[@]}"; do
        out="$TOOLS/$(basename "$f")"
        [ -x "$out" ] || { download "$base/$f" "$out"; chmod +x "$out"; }
    done
    export PATH="$TOOLS:$PATH"      # agar linuxdeploy menemukan plugin qt & appimage
    LINUXDEPLOY="$TOOLS/linuxdeploy-$ARCH.AppImage"
    # Tidak butuh FUSE: AppImage alat dijalankan dengan mengekstrak dulu.
    export APPIMAGE_EXTRACT_AND_RUN=1
}

# ================================================================ build C++
build_app() {
    log "Kompilasi $APP_NAME $APP_VERSION"
    need cmake
    rm -rf "$APPDIR"
    cmake -S "$ROOT" -B "$WORK/build" -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
    cmake --build "$WORK/build" -j"$(nproc)"
    DESTDIR="$APPDIR" cmake --install "$WORK/build"
    # Bersihkan sisa dari folder bridge hasil install.
    find "$APPDIR" -type d -name __pycache__ -prune -exec rm -rf {} +
}

# ================================================================ .desktop & metainfo
write_metadata() {
    log "Membuat .desktop dan metainfo dari src/version.h"
    local appsdir="$APPDIR/usr/share/applications"
    local metadir="$APPDIR/usr/share/metainfo"
    mkdir -p "$appsdir" "$metadir"

    cat > "$appsdir/$APP_BIN.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=$APP_NAME
GenericName=Music Player
Comment=$APP_DESC_EN
Comment[id]=$APP_DESC_ID
Exec=$APP_BIN
Icon=$APP_BIN
Categories=AudioVideo;Audio;Player;
Keywords=music;player;youtube;
Terminal=false
StartupWMClass=$APP_BIN
X-AppImage-Name=$APP_NAME
X-AppImage-Version=$APP_VERSION
X-AppImage-Vendor=$APP_DEVELOPER
EOF

    cat > "$metadir/$APP_ID.appdata.xml" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<component type="desktop-application">
  <id>$APP_ID</id>
  <metadata_license>CC0-1.0</metadata_license>
  <project_license>$APP_LICENSE</project_license>
  <name>$APP_NAME</name>
  <summary>$APP_DESC_EN</summary>
  <developer id="org.oogr">
    <name>$APP_DEVELOPER</name>
  </developer>
  <description>
    <p>$APP_DESC_EN</p>
    <p>Developer: $APP_DEVELOPER</p>
    <p>Maintainer: $APP_MAINTAINER</p>
  </description>
  <launchable type="desktop-id">$APP_BIN.desktop</launchable>
  <provides>
    <binary>$APP_BIN</binary>
  </provides>
  <releases>
    <release version="$APP_VERSION" date="$(date +%F)"/>
  </releases>
  <content_rating type="oars-1.1"/>
</component>
EOF
    [ "$APP_LICENSE" = "LicenseRef-OpenSource" ] && \
        warn "Lisensi di metainfo masih placeholder. Set BC_LICENSE=<ID SPDX>, mis. BC_LICENSE=GPL-3.0-or-later."
    return 0
}

# ================================================================ linuxdeploy (Qt)
deploy_qt() {
    log "linuxdeploy + plugin Qt (membundel Qt, plugin multimedia, dan dependensinya)"
    local plugins_dir
    plugins_dir="$("$QMAKE" -query QT_INSTALL_PLUGINS)"

    local extra=()
    [ -f "$plugins_dir/platforms/libqwayland-generic.so" ] && extra+=("libqwayland-generic.so")
    [ -f "$plugins_dir/platforms/libqwayland-egl.so" ]     && extra+=("libqwayland-egl.so")
    if [ ${#extra[@]} -gt 0 ]; then
        EXTRA_PLATFORM_PLUGINS="$(IFS=';'; echo "${extra[*]}")"
        export EXTRA_PLATFORM_PLUGINS
    fi
    export DEPLOY_PLATFORM_THEMES=1
    # strip bawaan linuxdeploy sering gagal pada pustaka Qt resmi (.relr.dyn).
    export NO_STRIP="${NO_STRIP:-1}"

    "$LINUXDEPLOY" --appdir "$APPDIR" \
        --executable "$APPDIR/usr/bin/$APP_BIN" \
        --desktop-file "$APPDIR/usr/share/applications/$APP_BIN.desktop" \
        --icon-file "$ROOT/resources/blackcurrant.png" --icon-filename "$APP_BIN" \
        --plugin qt

    if ! find "$APPDIR" -name 'libffmpegmediaplugin.so' | grep -q .; then
        warn "Plugin multimedia FFmpeg tidak ikut terbundel. Pastikan modul qtmultimedia terpasang di Qt (--install-qt)."
    fi
}

# ================================================================ Python portable
# Cari URL tarball python-build-standalone. Coba API GitHub dulu; bila terkena
# rate limit, pakai halaman rilis (tanpa API).
find_python_url() {
    local repo="astral-sh/python-build-standalone" api_body="" html_body="" tag="" suffix re url=""

    api_body="$(curl -fsSL --retry 2 ${AUTH_HEADER[@]+"${AUTH_HEADER[@]}"} \
        "https://api.github.com/repos/$repo/releases/latest" 2>/dev/null || true)"
    if [ -z "$api_body" ] || printf '%s' "$api_body" | grep -q 'rate limit'; then
        api_body=""
        tag="$(curl -fsSIL -o /dev/null -w '%{url_effective}' \
            "https://github.com/$repo/releases/latest" 2>/dev/null | sed 's#.*/tag/##' || true)"
        [ -n "$tag" ] && html_body="$(curl -fsSL --retry 2 \
            "https://github.com/$repo/releases/expanded_assets/$tag" 2>/dev/null || true)"
    fi

    # Utamakan varian _stripped (jauh lebih kecil), lalu varian biasa.
    for suffix in install_only_stripped install_only; do
        re="cpython-${PY_SERIES//./\\.}\\.[0-9]*[^\"/]*-${PY_TRIPLE}-${suffix}\\.tar\\.gz"
        if [ -n "$api_body" ]; then
            url="$(printf '%s' "$api_body" | grep -o "https://[^\"]*$re" | head -n1 || true)"
        else
            url="$(printf '%s' "$html_body" | grep -o "href=\"[^\"]*$re\"" | head -n1 \
                | sed 's#^href="##; s#"$##' || true)"
            case "$url" in /*) url="https://github.com$url" ;; esac
        fi
        [ -n "$url" ] && break
    done
    printf '%s' "$url"
}

bundle_python() {
    log "Membundel Python $PY_SERIES portable + paket bridge"
    need curl; need tar
    local py_dest="$APPDIR/usr/python"
    local tarball="$WORK/python-standalone.tar.gz"
    local url="${PYTHON_URL:-}"

    mkdir -p "$APPDIR/usr"
    if [ -z "$url" ]; then
        info "mencari rilis python-build-standalone terbaru…"
        url="$(find_python_url)"
        [ -n "$url" ] || die "Aset Python $PY_SERIES untuk $PY_TRIPLE tidak ditemukan. Set PYTHON_URL manual."
    fi
    download "$url" "$tarball"

    rm -rf "$py_dest" "$WORK/py-extract"
    mkdir -p "$WORK/py-extract"
    tar -xzf "$tarball" -C "$WORK/py-extract"
    mv "$WORK/py-extract/python" "$py_dest"
    rm -rf "$WORK/py-extract"

    info "pip install -r bridge/requirements.txt"
    "$py_dest/bin/python3" -m pip install --no-cache-dir --disable-pip-version-check \
        --no-warn-script-location -r "$ROOT/bridge/requirements.txt"

    info "memangkas file yang tidak dibutuhkan"
    find "$py_dest" -type d -name __pycache__ -prune -exec rm -rf {} +
    rm -rf "$py_dest"/include "$py_dest"/share \
           "$py_dest"/lib/python*/test "$py_dest"/lib/python*/idlelib \
           "$py_dest"/lib/python*/tkinter "$py_dest"/lib/python*/turtledemo \
           "$py_dest"/lib/python*/config-* \
           "$py_dest"/lib/python*/lib-dynload/_tkinter*.so \
           "$py_dest"/lib/tcl* "$py_dest"/lib/tk* "$py_dest"/lib/itcl* \
           "$py_dest"/lib/tdbc* "$py_dest"/lib/thread[0-9]* \
           "$py_dest"/bin/pip* "$py_dest"/bin/idle* "$py_dest"/bin/2to3* "$py_dest"/bin/pydoc*

    "$py_dest/bin/python3" -c \
        'import ytmusicapi, requests, PIL; print("    python OK:", __import__("sys").version.split()[0], "| ytmusicapi", ytmusicapi.__version__ if hasattr(ytmusicapi, "__version__") else "?")' \
        || die "Uji impor paket Python gagal."
}

# ================================================================ yt-dlp
bundle_ytdlp() {
    log "Membundel yt-dlp"
    need curl
    local dest="$APPDIR/usr/bin/yt-dlp"
    mkdir -p "$APPDIR/usr/bin"
    download "https://github.com/yt-dlp/yt-dlp/releases/latest/download/$YTDLP_ASSET" "$dest"
    chmod +x "$dest"
    info "versi yt-dlp: $("$dest" --version 2>/dev/null || echo 'tidak bisa dijalankan di host ini')"
}

# ================================================================ kemas AppImage
package_appimage() {
    log "Mengemas AppImage"
    mkdir -p "$DIST"
    export LINUXDEPLOY_OUTPUT_VERSION="$APP_VERSION"
    export LDAI_NO_APPSTREAM="${LDAI_NO_APPSTREAM:-1}"   # lisensi placeholder bisa gagal validasi

    local before
    before="$(mktemp)"; touch "$before"
    ( cd "$DIST" && "$LINUXDEPLOY" --appdir "$APPDIR" --output appimage )

    local out
    out="$(find "$DIST" -maxdepth 1 -name '*.AppImage' -newer "$before" -print | head -n1)"
    rm -f "$before"
    [ -n "$out" ] || die "AppImage tidak dihasilkan. Periksa log di atas."

    local final="$DIST/${FILE_BASE}-${APP_VERSION}-${ARCH}.AppImage"
    [ "$out" = "$final" ] || mv -f "$out" "$final"
    chmod +x "$final"
    ( cd "$DIST" && sha256sum "$(basename "$final")" > "$(basename "$final").sha256" )

    log "Selesai"
    info "$APP_NAME  $APP_VERSION"
    info "Developer : $APP_DEVELOPER"
    info "Maintainer: $APP_MAINTAINER"
    info "Berkas    : $final  ($(du -h "$final" | cut -f1))"
    info "Jalankan  : $final"
    info "Data user : ~/.config/blackcurrant-music-player/"
}

# ================================================================ main
# Bila di-source (untuk uji fungsi), jangan jalankan build.
[ "${BASH_SOURCE[0]}" = "$0" ] || return 0

if [ -n "$ONLY" ]; then
    mkdir -p "$WORK"
    case "$ONLY" in
        python) bundle_python ;;
        ytdlp)  bundle_ytdlp ;;
        *) die "--only: pilih 'python' atau 'ytdlp'." ;;
    esac
    exit 0
fi

[ "$CLEAN" -eq 1 ] && { log "Membersihkan $WORK"; rm -rf "$WORK"; }
mkdir -p "$WORK"

for c in curl tar cmake; do need "$c"; done
[ "$INSTALL_QT" -eq 1 ] && [ -z "${QT_DIR:-}" ] && install_qt
resolve_qt
fetch_tools
build_app
write_metadata
deploy_qt
bundle_python
bundle_ytdlp
package_appimage
