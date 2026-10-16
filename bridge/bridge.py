#!/usr/bin/env python3
"""Jembatan JSON-lines antara UI Qt/C++ dan `ytmusicapi`.

Proses ini berjalan lama. Ia membaca satu objek JSON per baris dari stdin,
menjalankannya lewat layanan Python yang sama persis dengan versi Tkinter
(`pymusic.ytmusic.YTMusicService`), lalu menulis satu objek JSON per baris
ke stdout.

Protokol
--------
Permintaan : {"id": <int>, "method": "<nama>", "params": { ... }}
Respons    : {"id": <int>, "ok": true,  "result": <apa saja>}
             {"id": <int>, "ok": false, "error": "<pesan>"}

Semua panggilan jaringan dijalankan di kumpulan thread agar tidak saling
memblokir, sementara objek YTMusic tetap dilindungi kunci internal.
"""

from __future__ import annotations

import json
import os
import sys
import threading
from concurrent.futures import ThreadPoolExecutor

# Pastikan folder bridge ada di sys.path agar `pymusic` bisa diimpor.
HERE = os.path.dirname(os.path.abspath(__file__))
if HERE not in sys.path:
    sys.path.insert(0, HERE)

from pymusic.ytmusic import NetworkError, YTMusicService  # noqa: E402

_SERVICE: YTMusicService | None = None
_SERVICE_LOCK = threading.Lock()
_OUT_LOCK = threading.Lock()


def get_service() -> YTMusicService:
    global _SERVICE
    with _SERVICE_LOCK:
        if _SERVICE is None:
            _SERVICE = YTMusicService()
        return _SERVICE


def _tracks(items):
    return [t.to_dict() for t in (items or [])]


# ---------------------------------------------------------------------------
# Metode yang dipanggil dari C++. Setiap nilai menerima (service, params).
# ---------------------------------------------------------------------------
def _search_all(svc: YTMusicService, p):
    data = svc.search_all(p["query"])
    return {"query": data["query"], "songs": _tracks(data["songs"]), "albums": data["albums"]}


def _search_songs(svc: YTMusicService, p):
    return _tracks(svc.search_songs(p["query"], p.get("limit", 20)))


def _search_albums(svc: YTMusicService, p):
    return svc.search_albums(p["query"], p.get("limit", 12))


def _search_artists(svc: YTMusicService, p):
    return svc.search_artists(p["query"], p.get("limit", 10))


def _search_suggestions(svc: YTMusicService, p):
    return svc.search_suggestions(p["query"], p.get("limit", 8))


def _quick_picks(svc: YTMusicService, p):
    return _tracks(svc.quick_picks(p.get("limit", 16)))


def _artist_songs(svc: YTMusicService, p):
    return _tracks(svc.artist_songs(list(p.get("artists") or []), p.get("limit", 16)))


def _mood_categories(svc: YTMusicService, p):
    return svc.mood_categories()


def _home_shelves(svc: YTMusicService, p):
    return svc.home_shelves(p.get("limit", 3))


def _home_songs(svc: YTMusicService, p):
    return _tracks(svc.home_songs(set(p.get("exclude") or []), p.get("limit", 12)))


def _genre_tracks(svc: YTMusicService, p):
    return _tracks(svc.genre_tracks(p.get("params", ""), p.get("fallback_query", "")))


def _get_artist(svc: YTMusicService, p):
    return svc.get_artist(p["browse_id"])


def _album_tracks(svc: YTMusicService, p):
    data = svc.album_tracks(p["browse_id"])
    out = dict(data)
    out["tracks"] = _tracks(data.get("tracks"))
    return out


def _get_playlist(svc: YTMusicService, p):
    return svc.get_playlist(p["browse_id"], p.get("limit", 100))


def _lyrics(svc: YTMusicService, p):
    return svc.lyrics(p["video_id"])


def _watch_playlist(svc: YTMusicService, p):
    return _tracks(svc.watch_playlist(p["video_id"], p.get("limit", 25)))


def _recommendations(svc: YTMusicService, p):
    return _tracks(svc.recommendations(list(p.get("seeds") or []),
                                       set(p.get("exclude") or []),
                                       p.get("limit", 12)))


def _ping(svc: YTMusicService, p):
    return {"pong": True, "pid": os.getpid()}


METHODS = {
    "search_all": _search_all,
    "search_songs": _search_songs,
    "search_albums": _search_albums,
    "search_artists": _search_artists,
    "search_suggestions": _search_suggestions,
    "quick_picks": _quick_picks,
    "artist_songs": _artist_songs,
    "mood_categories": _mood_categories,
    "home_shelves": _home_shelves,
    "home_songs": _home_songs,
    "get_artist": _get_artist,
    "genre_tracks": _genre_tracks,
    "album_tracks": _album_tracks,
    "get_playlist": _get_playlist,
    "lyrics": _lyrics,
    "watch_playlist": _watch_playlist,
    "recommendations": _recommendations,
    "ping": _ping,
}


def _write(obj) -> None:
    line = json.dumps(obj, ensure_ascii=False)
    with _OUT_LOCK:
        sys.stdout.write(line + "\n")
        sys.stdout.flush()


def _handle(msg: dict) -> None:
    req_id = msg.get("id")
    method = msg.get("method")
    params = msg.get("params") or {}
    fn = METHODS.get(method)
    if fn is None:
        _write({"id": req_id, "ok": False, "error": f"metode tidak dikenal: {method}"})
        return
    try:
        svc = get_service()
        result = fn(svc, params)
        _write({"id": req_id, "ok": True, "result": result})
    except NetworkError as exc:
        _write({"id": req_id, "ok": False, "error": str(exc), "kind": "network"})
    except Exception as exc:  # noqa: BLE001 - jangan pernah matikan jembatan
        _write({"id": req_id, "ok": False, "error": f"{type(exc).__name__}: {exc}"})


def main() -> int:
    pool = ThreadPoolExecutor(max_workers=8)
    for raw in sys.stdin:
        raw = raw.strip()
        if not raw:
            continue
        try:
            msg = json.loads(raw)
        except Exception as exc:  # noqa: BLE001
            _write({"id": None, "ok": False, "error": f"JSON tidak valid: {exc}"})
            continue
        if msg.get("method") == "__quit__":
            break
        pool.submit(_handle, msg)
    pool.shutdown(wait=False, cancel_futures=True)
    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        sys.exit(0)
