"""Layanan YouTube Music berbasis `ytmusicapi` (tanpa API key resmi).

Semua metode mengembalikan model `Track` / dict sederhana sehingga lapisan
UI tidak perlu tahu bentuk mentah respons API.
"""

from __future__ import annotations

import concurrent.futures
import threading
from typing import Any, Optional

import requests
from ytmusicapi import YTMusic

from .models import Track, normalize_tracks, upscale_thumb

THUMB_TIMEOUT = 15
SEARCH_SONG_LIMIT = 20
SEARCH_ALBUM_LIMIT = 12


class NetworkError(RuntimeError):
    """Kesalahan jaringan / layanan YouTube Music."""


class YTMusicService:
    """Pembungkus tipis di sekitar YTMusic dengan penanganan galat rapi.

    Semua panggilan jaringan diserialkan lewat satu kunci karena objek
    YTMusic (beserta sesi HTTP internalnya) tidak dirancang untuk dipakai
    bersamaan dari beberapa thread.
    """

    def __init__(self) -> None:
        try:
            self._api = YTMusic()
        except Exception as exc:  # pragma: no cover - kegagalan init langka
            raise NetworkError(f"Gagal menginisialisasi YouTube Music: {exc}") from exc
        self._executor = concurrent.futures.ThreadPoolExecutor(max_workers=4)
        self._lock = threading.RLock()

    def _call(self, fn, *args, **kwargs):
        """Jalankan panggilan API dengan penjagaan thread."""
        with self._lock:
            return fn(*args, **kwargs)

    # ---------------- beranda ----------------
    def quick_picks(self, limit: int = 12) -> list[Track]:
        """'Pilihan cepat' dari beranda YouTube Music."""
        try:
            shelves = self._call(self._api.get_home, limit=1)
        except Exception as exc:
            raise NetworkError(f"Gagal memuat pilihan cepat: {exc}") from exc
        for shelf in shelves:
            if "quick pick" in (shelf.get("title") or "").lower():
                tracks = normalize_tracks(shelf.get("contents", []))
                if tracks:
                    return tracks[:limit]
        # fallback: gabungkan semua isi shelf
        tracks = []
        for shelf in shelves:
            tracks.extend(normalize_tracks(shelf.get("contents", [])))
        return tracks[:limit]

    def artist_songs(self, artists: list[dict[str, Any]], limit: int = 16) -> list[Track]:
        """Lagu-lagu populer dari beberapa artis (mis. artis yang paling sering
        diputar). Hasil digilir antar-artis agar bervariasi."""
        per_artist: list[list[Track]] = []
        for artist in artists or []:
            name = (artist.get("name") or "").strip()
            browse_id = artist.get("id") or ""
            tracks: list[Track] = []
            if browse_id:
                try:
                    data = self._call(self._api.get_artist, browse_id)
                    songs = ((data or {}).get("songs") or {}).get("results") or []
                    tracks = normalize_tracks(songs)
                except Exception:
                    tracks = []
            if len(tracks) < 5 and name:  # lengkapi lewat pencarian
                try:
                    found = self.search_songs(name, limit=15)
                except NetworkError:
                    found = []
                wanted = name.lower()
                have = {t.video_id for t in tracks}
                for t in found:
                    if t.video_id in have:
                        continue
                    if any(wanted == a.lower() for a in t.artists):
                        tracks.append(t)
                        have.add(t.video_id)
            per_artist.append(tracks)

        out: list[Track] = []
        seen: set[str] = set()
        depth = 0
        while len(out) < limit and any(depth < len(lst) for lst in per_artist):
            for lst in per_artist:
                if depth < len(lst) and lst[depth].video_id not in seen:
                    seen.add(lst[depth].video_id)
                    out.append(lst[depth])
                    if len(out) >= limit:
                        break
            depth += 1
        return out

    def home_shelves(self, limit: int = 2) -> list[dict[str, Any]]:
        """Shelves beranda (pilihan cepat, album, playlist editorial, ...)."""
        try:
            return self._call(self._api.get_home, limit=limit)
        except Exception as exc:
            raise NetworkError(f"Gagal memuat beranda: {exc}") from exc

    # ---------------- pencarian ----------------
    def search_songs(self, query: str, limit: int = SEARCH_SONG_LIMIT) -> list[Track]:
        try:
            results = self._call(self._api.search, query, filter="songs", limit=limit)
        except Exception as exc:
            raise NetworkError(f"Pencarian gagal: {exc}") from exc
        return normalize_tracks(results)

    def search_albums(self, query: str, limit: int = SEARCH_ALBUM_LIMIT) -> list[dict[str, Any]]:
        try:
            results = self._call(self._api.search, query, filter="albums", limit=limit)
        except Exception as exc:
            raise NetworkError(f"Pencarian album gagal: {exc}") from exc
        albums = []
        for item in results or []:
            if not item.get("browseId"):
                continue
            thumbs = item.get("thumbnails") or []
            albums.append(
                {
                    "browse_id": item["browseId"],
                    "title": item.get("title") or "Album tanpa judul",
                    "artists": [a.get("name") for a in (item.get("artists") or []) if a.get("name")],
                    "year": item.get("year"),
                    "thumbnail": upscale_thumb((thumbs[-1] or {}).get("url") if thumbs else None),
                }
            )
        return albums

    def search_suggestions(self, query: str, limit: int = 8) -> list[str]:
        """Rekomendasi kata kunci (judul lagu / nama artis) saat mengetik."""
        query = (query or "").strip()
        if not query:
            return []
        try:
            raw = self._call(self._api.get_search_suggestions, query)
        except Exception:
            return []
        out: list[str] = []
        for item in raw or []:
            if isinstance(item, dict):
                text = item.get("text") or item.get("query") or ""
            else:
                text = str(item)
            text = text.strip()
            if not text or text.lower() == query.lower() or text in out:
                continue
            out.append(text)
            if len(out) >= limit:
                break
        return out

    # ---------------- artis ----------------
    def search_artists(self, query: str, limit: int = 10) -> list[dict[str, Any]]:
        """Cari artis; kembalikan daftar dict dengan browseId, nama, thumbnail."""
        try:
            results = self._call(self._api.search, query, filter="artists", limit=limit)
        except Exception as exc:
            raise NetworkError(f"Pencarian artis gagal: {exc}") from exc
        artists = []
        for item in results or []:
            if item.get("resultType") != "artist":
                continue
            thumbs = item.get("thumbnails") or []
            artists.append(
                {
                    "browse_id": item.get("browseId"),
                    "name": item.get("artist"),
                    "thumbnail": upscale_thumb((thumbs[-1] or {}).get("url") if thumbs else None),
                    "subscribers": item.get("subscribers"),
                }
            )
        return artists

    def get_artist(self, browse_id: str) -> dict[str, Any]:
        """Detail artis: top songs, album, info."""
        try:
            data = self._call(self._api.get_artist, browse_id)
        except Exception as exc:
            raise NetworkError(f"Gagal memuat artis: {exc}") from exc
        return data

    def search_all(self, query: str) -> dict[str, Any]:
        """Hasil gabungan: lagu + album dalam satu panggilan."""
        return {
            "query": query,
            "songs": self.search_songs(query),
            "albums": self.search_albums(query),
        }

    # ---------------- album ----------------
    def album_tracks(self, browse_id: str) -> dict[str, Any]:
        try:
            data = self._call(self._api.get_album, browse_id)
        except Exception as exc:
            raise NetworkError(f"Gagal memuat album: {exc}") from exc
        name = data.get("title")
        tracks = normalize_tracks(data.get("tracks", []), album_name=name)
        thumbs = data.get("thumbnails") or []
        return {
            "title": name,
            "artists": [a.get("name") for a in (data.get("artists") or []) if a.get("name")],
            "year": data.get("year"),
            "duration": data.get("duration") or data.get("duration_seconds"),
            "thumbnail": upscale_thumb((thumbs[-1] or {}).get("url") if thumbs else None),
            "tracks": tracks,
        }

    # ---------------- genre / mood ----------------
    def mood_categories(self) -> dict[str, list[dict[str, str]]]:
        """Kategori 'Moods & moments' dan 'Genres' beserta params-nya."""
        try:
            return self._call(self._api.get_mood_categories)
        except Exception as exc:
            raise NetworkError(f"Gagal memuat genre: {exc}") from exc

    def genre_tracks(self, params: str, fallback_query: str = "") -> list[Track]:
        """Lagu-lagu dari kategori mood/genre tertentu."""
        try:
            data = self._call(self._api.get_mood_playlists, params)
            tracks = normalize_tracks(data.get("tracks") or data.get("contents") or [])
            if tracks:
                return tracks
        except Exception:
            pass
        if fallback_query:  # pencarian sebagai cadangan
            return self.search_songs(fallback_query, limit=SEARCH_SONG_LIMIT)
        return []

    # ---------------- playlist / artist songs ----------------
    def get_playlist(self, browse_id: str, limit: int = 100) -> dict:
        """Ambil detail playlist/album/artist-songs via browseId.
        Mengembalikan dict dengan key 'tracks' (list of track dicts)."""
        try:
            data = self._call(self._api.get_playlist, browse_id, limit=limit)
        except Exception as exc:
            raise NetworkError(f"Gagal memuat playlist: {exc}") from exc
        return data

    # ---------------- lirik ----------------
    def lyrics(self, video_id: str) -> Optional[dict[str, Any]]:
        """Ambil lirik sebuah lagu bila tersedia di YouTube Music.

        Mengembalikan dict ``{"lyrics", "source", "has_timestamps"}`` atau
        ``None`` bila lagu tidak punya lirik.
        """
        video_id = (video_id or "").strip()
        if not video_id:
            return None
        try:
            data = self._call(self._api.get_watch_playlist, videoId=video_id)
        except Exception:
            return None
        browse_id = (data or {}).get("lyrics")
        if not browse_id:
            return None
        try:
            result = self._call(self._api.get_lyrics, browse_id)
        except Exception:
            return None
        if not result:
            return None
        if isinstance(result, dict):
            text = result.get("lyrics")
            source = result.get("source")
            has_ts = bool(result.get("hasTimestamps"))
        else:  # pragma: no cover - model objek (versi ytmusicapi lain)
            text = getattr(result, "lyrics", None)
            source = getattr(result, "source", None)
            has_ts = bool(getattr(result, "hasTimestamps", False))
        if not text or not text.strip():
            return None
        return {"lyrics": text, "source": source, "has_timestamps": has_ts}

    # ---------------- rekomendasi ----------------
    def watch_playlist(self, video_id: str, limit: int = 25) -> list[Track]:
        """'Radio' YouTube Music: lagu-lagu terkait untuk sebuah videoId."""
        try:
            data = self._call(self._api.get_watch_playlist, videoId=video_id, limit=limit)
        except Exception as exc:
            raise NetworkError(f"Gagal memuat rekomendasi: {exc}") from exc
        return normalize_tracks(data.get("tracks", []))

    def recommendations(self, seeds: list[str], exclude: set[str] | None = None,
                        limit: int = 12) -> list[Track]:
        """Kumpulkan rekomendasi dari beberapa 'benih' lagu (lagu disukai /
        riwayat). Lagu yang sudah ada di `exclude` dilewati."""
        exclude = set(exclude or set())
        seen = set(exclude)
        out: list[Track] = []
        for seed in seeds:
            try:
                tracks = self.watch_playlist(seed, limit=25)
            except NetworkError:
                continue
            for track in tracks:
                if not track.video_id or track.video_id in seen:
                    continue
                seen.add(track.video_id)
                out.append(track)
                if len(out) >= limit:
                    return out
        return out

    def home_songs(self, exclude: set[str] | None = None, limit: int = 12,
                   skip_titles: tuple[str, ...] = ("quick pick",)) -> list[Track]:
        """Lagu dari shelf beranda YouTube Music (selain yang sudah dipakai
        bagian lain), berguna sebagai rekomendasi cadangan."""
        exclude = set(exclude or set())
        seen = set(exclude)
        out: list[Track] = []
        try:
            shelves = self.home_shelves(limit=3)
        except NetworkError:
            return out
        for shelf in shelves:
            if any(skip in (shelf.get("title") or "").lower() for skip in skip_titles):
                continue
            for track in normalize_tracks(shelf.get("contents", [])):
                if not track.video_id or track.video_id in seen:
                    continue
                seen.add(track.video_id)
                out.append(track)
                if len(out) >= limit:
                    return out
        return out

    # ---------------- asinkron ----------------
    def submit(self, fn, on_success=None, on_error=None) -> concurrent.futures.Future:
        """Jalankan `fn` di thread terpisah; callback dipanggil di main thread
        melalui wrapper yang sudah disediakan UI (lihat app.run_async)."""

        def runner():
            try:
                return fn()
            except NetworkError:
                raise
            except Exception as exc:
                raise NetworkError(str(exc)) from exc

        return self._executor.submit(runner)


class ThumbnailLoader:
    """Unduh + cache thumbnail di disk, konversi ke PhotoImage Tk."""

    def __init__(self, cache_dir, post=None) -> None:
        self.cache_dir = cache_dir
        self.cache_dir.mkdir(parents=True, exist_ok=True)
        self._executor = concurrent.futures.ThreadPoolExecutor(max_workers=6)
        self._mem: dict[str, Any] = {}
        self._post = post  # penjadwal main-thread (opsional)

    def _path_for(self, url: str):
        import hashlib

        key = hashlib.md5(url.encode("utf-8")).hexdigest()
        return self.cache_dir / f"{key}.jpg"

    def fetch_bytes(self, url: str) -> Optional[bytes]:
        """Unduh (atau ambil dari cache) berkas thumbnail."""
        if not url:
            return None
        path = self._path_for(url)
        if path.exists():
            try:
                return path.read_bytes()
            except OSError:
                pass
        try:
            resp = requests.get(url, timeout=THUMB_TIMEOUT)
            resp.raise_for_status()
            path.write_bytes(resp.content)
            return resp.content
        except Exception:
            return None

    def load_image(self, url: str, size: int):
        """Kembalikan PIL Image ukuran tetap; None bila gagal."""
        from PIL import Image
        import io

        data = self.fetch_bytes(url)
        if not data:
            return None
        try:
            img = Image.open(io.BytesIO(data)).convert("RGB")
        except Exception:
            return None
        ratio = max(size / img.width, size / img.height)
        img = img.resize((max(1, int(img.width * ratio)), max(1, int(img.height * ratio))), Image.LANCZOS)
        left = (img.width - size) // 2
        top = (img.height - size) // 2
        return img.crop((left, top, left + size, top + size))

    def load_bytes_async(self, url: str, callback) -> None:
        """Unduh di background, lalu panggil `callback(bytes|None)`.

        Callback dijalankan di main thread (lewat `post`) karena pembuat
        PhotoImage Tk tidak boleh dilakukan dari thread lain.
        """
        def work():
            data = self.fetch_bytes(url)
            if self._post is not None:
                self._post(lambda: callback(data))
            else:
                callback(data)

        self._executor.submit(work)
