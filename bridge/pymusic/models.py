"""Model data lagu (Track) beserta normalisasi dari berbagai bentuk
respons YouTube Music (ytmusicapi).
"""

from __future__ import annotations

from dataclasses import dataclass, field, asdict
from typing import Any, Iterable, Optional


def parse_duration(value: Any) -> Optional[int]:
    """Ubah durasi apa pun menjadi detik (int) atau None.

    Mendukung bentuk: "3:45", "1:02:03", 225, "225", None.
    """
    if value is None or value == "":
        return None
    if isinstance(value, (int, float)):
        return int(value)
    value = str(value).strip()
    if value.isdigit():
        return int(value)
    parts = value.split(":")
    try:
        parts = [int(p) for p in parts]
    except ValueError:
        return None
    seconds = 0
    for part in parts:
        seconds = seconds * 60 + part
    return seconds


def fmt_duration(seconds: Optional[int]) -> str:
    """Format detik menjadi 'm:ss' atau 'h:mm:ss'."""
    if seconds is None:
        return "--:--"
    seconds = max(0, int(seconds))
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    if h:
        return f"{h}:{m:02d}:{s:02d}"
    return f"{m}:{s}"


def _names(items: Any) -> tuple[list[str], list[str]]:
    """Ekstrak daftar nama artis dan ID dari berbagai bentuk respons.
    Returns (names, ids)"""
    if not items:
        return [], []
    names = []
    ids = []
    for item in items:
        if isinstance(item, dict):
            name = item.get("name")
            if name:
                names.append(name)
            aid = item.get("id") or item.get("browseId")
            if aid:
                ids.append(aid)
        elif isinstance(item, str):
            names.append(item)
    return names, ids


def _best_thumb(thumbnails: Any, min_size: int = 120) -> Optional[str]:
    """Pilih URL thumbnail beresolusi terbaik."""
    if not thumbnails:
        return None
    best = None
    for thumb in thumbnails:
        if not isinstance(thumb, dict):
            continue
        width = thumb.get("width") or 0
        if best is None or width >= best[0]:
            best = (width, thumb.get("url"))
    if best and best[1]:
        return best[1]
    return None


def upscale_thumb(url: Optional[str]) -> Optional[str]:
    """Minta thumbnail beresolusi lebih besar ke server YouTube.

    URL yt3.googleusercontent.com mendukung parameter `=wNNN`; kita ganti
    ke w480 agar tampil tajam di kartu besar.
    """
    if not url:
        return None
    base = url.split("=")[0]
    return base + "=w480-h480-l90-rj"


@dataclass
class Track:
    """Representasi satu lagu yang konsisten di seluruh aplikasi."""

    video_id: str
    title: str
    artists: list[str] = field(default_factory=list)
    artist_ids: list[str] = field(default_factory=list)  # browseId per artis
    album: Optional[str] = None
    duration: Optional[int] = None  # detik
    thumbnail: Optional[str] = None

    # -- konversi ----------------------------------------------------------
    @classmethod
    def from_search(cls, item: dict[str, Any]) -> "Track":
        names, ids = _names(item.get("artists"))
        return cls(
            video_id=item.get("videoId") or "",
            title=item.get("title") or "Tanpa judul",
            artists=names,
            artist_ids=ids,
            album=(item.get("album") or {}).get("name") if isinstance(item.get("album"), dict) else item.get("album"),
            duration=parse_duration(item.get("duration")),
            thumbnail=_best_thumb(item.get("thumbnails")),
        )

    @classmethod
    def from_home(cls, item: dict[str, Any]) -> "Track":
        return cls.from_search(item)

    @classmethod
    def from_album_track(cls, item: dict[str, Any], album_name: Optional[str] = None) -> "Track":
        track = cls.from_search(item)
        if not track.album:
            track.album = album_name
        return track

    @classmethod
    def from_dict(cls, data: dict[str, Any]) -> "Track":
        return cls(
            video_id=data.get("video_id", ""),
            title=data.get("title", "Tanpa judul"),
            artists=list(data.get("artists") or []),
            artist_ids=list(data.get("artist_ids") or []),
            album=data.get("album"),
            duration=parse_duration(data.get("duration")),
            thumbnail=data.get("thumbnail"),
        )

    def to_dict(self) -> dict[str, Any]:
        return {
            "video_id": self.video_id,
            "title": self.title,
            "artists": self.artists,
            "artist_ids": self.artist_ids,
            "album": self.album,
            "duration": self.duration,
            "thumbnail": self.thumbnail,
        }

    # -- properti tampilan -------------------------------------------------
    @property
    def artist_str(self) -> str:
        return ", ".join(self.artists) if self.artists else "Artis tidak diketahui"

    @property
    def display_duration(self) -> str:
        return fmt_duration(self.duration)

    @property
    def big_thumbnail(self) -> Optional[str]:
        return upscale_thumb(self.thumbnail)


def normalize_tracks(items: Iterable[dict[str, Any]], album_name: Optional[str] = None) -> list[Track]:
    """Ubah kumpulan dict respons API menjadi daftar Track valid."""
    tracks: list[Track] = []
    for item in items or []:
        if not isinstance(item, dict):
            continue
        track = Track.from_album_track(item, album_name) if album_name else Track.from_search(item)
        if track.video_id:
            tracks.append(track)
    return tracks
