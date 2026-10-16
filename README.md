# Blackcurrant Music Player — Qt/C++

**Versi: 0.1.0-Beta** · Developer: Opensource OS Gathering Republic (OOGR) ·
Maintainer: Didi Sloth Stanca

Versi Qt 6 + C++ dari pemutar musik gaya Spotify yang sebelumnya ditulis dengan
Python + Tkinter (`../music_player/`). Tampilan dan perilaku dibuat semirip
mungkin, tetapi seluruh antarmuka dan pemutaran audio berjalan di C++/Qt agar
jauh lebih cepat dan mulus.

## Arsitektur

```
+--------------------------- Qt / C++ ---------------------------+
|  MainWindow  (sidebar, topbar, konten, bilah pemutar, lirik)    |
|  Views       (beranda, pencarian, koleksi, detail, artis, ...)   |
|  Widgets     (kartu lagu, baris lagu, ubin genre, area gulir)    |
|  Theme       (blackcurrant / dark / light, QSS)                  |
|  Storage     (playlist, disukai, riwayat, setelan -> JSON)       |
|  Thumbnails  (cache memori + disk)                               |
|  AudioPlayer (QMediaPlayer + FFmpeg, resolusi via yt-dlp)        |
+---------------------------------+--------------------------------+
                                  | JSON lines (stdin/stdout)
+---------------------------------v--------------------------------+
|  Bridge Python (bridge/bridge.py) -> YTMusicService (ytmusicapi) |
+------------------------------------------------------------------+
```

- **UI & audio 100% C++/Qt.** Tidak ada Tkinter.
- **Data musik tetap lewat Python.** Proses `bridge/bridge.py` yang berjalan lama
  berbicara protokol JSON-lines dan memakai ulang lapisan `ytmusicapi` yang sama
  persis dengan aplikasi Python, sehingga hasil pencarian/album/artis identik.
- **Audio memakai Qt Multimedia** (backend FFmpeg). URL stream diambil dengan
  `yt-dlp -g` (tanpa dependensi tambahan).

## Kebutuhan

- Qt 6 (Widgets, Network, Multimedia) — diuji pada Qt 6.8
- CMake >= 3.16, Ninja (opsional), kompiler C++17
- `python3` + `ytmusicapi`
- `yt-dlp` di `PATH` (atau `~/.local/bin/yt-dlp`, atau set `BC_YTDLP`)

## Membangun & menjalankan

```bash
cd music_player_qt
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C build
./build/blackcurrant
```

Folder `bridge/` otomatis disalin ke direktori build saat kompilasi.

## Bahasa / Language

Antarmuka tersedia dalam **English** (bawaan) dan **Bahasa Indonesia**. Ganti lewat
pemilih **Bahasa / Language** di bagian bawah sidebar (di bawah pemilih Tema); pilihan
disimpan di `settings.json` dan langsung diterapkan tanpa memulai ulang.

- Teks sumber di kode berbahasa Indonesia dan dibungkus `TR("...")`
  (`src/i18n.h`). Padanan Inggris ada di tabel `src/i18n_table.inc`.
- Menambah/mengubah terjemahan: tambahkan baris `{"teks Indonesia", "English text"},`
  pada `src/i18n_table.inc`. Teks yang tidak ada di tabel ditampilkan apa adanya.

## Tema

Tiga tema, dapat diganti dari pemilih **Tema** di bagian bawah sidebar:

| Tema          | Latar                          | Aksen                     |
|---------------|--------------------------------|---------------------------|
| `blackcurrant` (bawaan) | ungu sangat gelap   | ungu magenta `#a855f7`    |
| `dark`        | hitam netral                   | rose/oranye hangat `#ff7a59` |
| `light`       | putih                          | ungu tua `#7c3aed`        |

Tidak ada aksen hijau maupun biru. Pilihan tema disimpan di setelan.

## Versi & About

Informasi aplikasi (nama, versi, pengembang, pemelihara) dipusatkan di
`src/version.h`. Ubah di sana saja: judul window, judul sidebar, dan halaman
**Tentang / About** (item terakhir di menu sidebar) ikut berubah.

## Build AppImage (standalone)

```bash
./build-appimage.sh --install-qt          # unduh Qt 6.7.3 (aqtinstall) lalu build
QT_DIR=$HOME/Qt/6.7.3/gcc_64 ./build-appimage.sh   # atau pakai Qt sendiri (>= 6.5)
BC_LICENSE=GPL-3.0-or-later ./build-appimage.sh    # isi ID SPDX lisensi untuk metainfo
```

Hasil: `dist/Blackcurrant_Music_Player-<versi>-<arch>.AppImage` (+ `.sha256`).

Yang dibundel: Qt 6 + plugin Multimedia (linuxdeploy + plugin Qt), Python portable
beserta `ytmusicapi`/`requests`/`Pillow` (`bridge/requirements.txt`), dan `yt-dlp`.
Nama, versi, developer, dan maintainer pada `.desktop` serta metainfo diambil dari
`src/version.h` (sama dengan halaman About). Data pengguna tetap di
`~/.config/blackcurrant-music-player/`, bukan di dalam AppImage.

Catatan: build di distro paling lama yang ingin didukung (glibc lebih tua = lebih
kompatibel).

### Pembaruan otomatis yt-dlp

AppImage bersifat hanya-baca, jadi saat pertama dipakai aplikasi menyalin `yt-dlp`
bawaan ke `~/.config/blackcurrant-music-player/bin/yt-dlp`. Salinan itu diperbarui
sendiri (`yt-dlp -U`):

- di latar belakang ±8 detik setelah aplikasi dibuka, paling sering sehari sekali
  (penanda waktu: `bin/.last-update`);
- otomatis sekali per sesi bila semua klien YouTube gagal mengambil stream, lalu
  pemutaran dicoba lagi.

Bila AppImage baru dibangun ulang dengan yt-dlp yang lebih baru dari salinan, salinan
diganti. yt-dlp dari sistem atau dari `BC_YTDLP` tidak disentuh. Pembaruan butuh
internet; bila gagal, aplikasi tetap memakai salinan yang ada.

## Fitur

- **Beranda** — sapaan, pintasan, pilihan cepat, rekomendasi personal,
  album dari beranda YouTube Music, genre/suasana hati, terakhir diputar.
  - *Terakhir Diputar*: maks. 16 lagu, 3 baris ke bawah × 3 kolom terlihat;
    sisanya digeser ke samping (scrollbar, Shift+roda, atau touchpad).
  - *Pilihan Cepat*: lagu dari artis yang paling sering kamu putar (belum ada
    riwayat → pilihan cepat bawaan YouTube Music).
  - *Rekomendasi*: dicampur dari genre yang paling sering kamu putar + lagu
    serupa dari lagu favoritmu.
  - *Genre Lagu*: diurutkan dari genre yang paling sering kamu putar.
- **Pencarian** — saran ketik-langsung, riwayat pencarian, hasil lagu + album.
- **Koleksi** — playlist lokal (buat/ganti nama/hapus), lagu disukai,
  album disukai, riwayat musik (dapat dibersihkan).
- **Detail** — album, playlist lokal, dan halaman genre.
- **Artis** — lagu terpopuler, album & single, semua lagu.
- **Panel lirik** — meluncur dari kanan, memuat lirik lagu yang diputar.
- **Bilah pemutar** — putar/jeda, sebelumnya/berikutnya, acak, ulangi satu lagu,
  geser posisi, suka, volume (tersimpan), pemilih perangkat audio, uji suara.
- **Sidebar** — laci yang tertutup secara bawaan; buka dengan ☰. Menutup
  otomatis setelah memilih menu, klik area gelap, atau `Esc`. Berisi daftar
  playlist lokal, status audio, status jaringan, dan pemilih tema.
- **Tombol kembali** dan tumpukan navigasi.
- **Menu konteks lagu** — putar sekarang, putar berikutnya, suka, tambahkan ke
  playlist, buka artis, hapus dari playlist.

### Pintasan papan tik

- `Spasi` — putar/jeda (diabaikan saat fokus di kolom teks)
- `Esc` — kembali ke halaman sebelumnya

## Direktori data

Disimpan di `~/.config/blackcurrant-music-player/` (mengikuti `XDG_CONFIG_HOME`).
Data lama di `<folder aplikasi>/data` disalin otomatis sekali saja. Berisi
`playlists.json`, `liked.json`, `liked_albums.json`, `history.json`,
`playcounts.json`, `taste.json` (profil artis & genre favorit),
`search_history.json`, `settings.json`, dan `cache/`.

### Bagaimana genre favorit dipelajari

YouTube Music tidak memberi tahu genre sebuah lagu, jadi genre dipelajari
secara lokal: (1) lagu yang diputar dari halaman genre dihitung untuk genre itu,
(2) isi halaman genre yang kamu buka mengaitkan para artisnya dengan genre
tersebut, sehingga lagu artis yang sama yang diputar dari tempat lain ikut
dihitung. Makin banyak kamu menjelajah/memutar lewat halaman genre, makin
akurat urutan genre dan rekomendasinya.
Setel `BC_DATA_DIR` untuk memakai lokasi lain.

## Variabel lingkungan

| Variabel         | Fungsi                                             |
|------------------|----------------------------------------------------|
| `BC_PYTHON`      | Interpreter Python untuk jembatan (bawaan `python3`) |
| `BC_YTDLP`       | Lokasi biner `yt-dlp`                              |
| `BC_DATA_DIR`    | Direktori data                                     |

## Protokol jembatan

Permintaan (satu baris JSON):

```json
{"id": 1, "method": "search_all", "params": {"query": "sheila on 7"}}
```

Balasan:

```json
{"id": 1, "ok": true, "result": { "songs": [...], "albums": [...] }}
{"id": 1, "ok": false, "error": "..."}
```

Metode yang tersedia: `ping`, `search_suggestions`, `search_songs`,
`search_albums`, `search_all`, `search_artists`, `quick_picks`,
`mood_categories`, `home_shelves`, `home_songs`, `recommendations`,
`get_artist`, `album_tracks`, `genre_tracks`, `get_playlist`, `lyrics`,
`watch_playlist`, `artist_songs`, dan `__quit__`.
