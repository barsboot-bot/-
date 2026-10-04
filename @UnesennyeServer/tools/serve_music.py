#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
«унесённые» — HTTP-зеркало внешней музыкальной библиотеки.

Раздаёт файлы из <Profile>/Music (или любой другой папки), чтобы клиенты
мода могли докачивать треки кассет/дисков через DownloadFile.
В UE_Config::libraryBaseURL укажите, например: http://ваш-сервер:8788/Music

Безопасность:
  * отдаются ТОЛЬКО аудио-расширения (.ogg .mp3 .wav);
  * защита от path traversal; скрытые файлы недоступны;
  * listing каталогов запрещён;
  * rate-limit: 50 ошибок (403/404) за минуту -> временный бан IP.

Запуск:
    python3 serve_music.py --root "D:/DayZServer/profiles/p1/Music" --port 8788
"""

import argparse
import os
import socketserver
import sys
import threading
import time
from http.server import SimpleHTTPRequestHandler

AUDIO_EXT = {".ogg": "audio/ogg", ".mp3": "audio/mpeg", ".wav": "audio/wav"}

_lock = threading.Lock()
_hits = {}            # ip -> [t_last_fail, fail_count]
BAN_TIME = 600        # секунд
FAIL_LIMIT = 50


class MusicHandler(SimpleHTTPRequestHandler):
    root = None

    def log_message(self, fmt, *args):
        sys.stdout.write("[music] %s - %s\n" % (self.address_string(), fmt % args))

    def _banned(self):
        ip = self.client_address[0]
        with _lock:
            rec = _hits.get(ip)
            if rec and rec[1] >= FAIL_LIMIT and (time.time() - rec[0]) < BAN_TIME:
                return True
        return False

    def _fail(self):
        ip = self.client_address[0]
        with _lock:
            rec = _hits.setdefault(ip, [0, 0])
            if time.time() - rec[0] > 60:
                rec[1] = 0
            rec[0] = time.time()
            rec[1] += 1

    def translate_path(self, path):
        """Только внутри корня Music, без traversal."""
        path = path.split("?", 1)[0].split("#", 1)[0]
        rel = os.path.normpath(path.lstrip("/")).replace("\\", "/")
        if rel.startswith("..") or os.path.isabs(rel):
            return None
        root = os.path.normpath(self.root)
        full = os.path.normpath(os.path.join(root, rel))
        if full != root and not full.startswith(root + os.sep):
            return None
        return full

    def do_GET(self):
        if self._banned():
            self.send_error(429, "rate limited")
            return
        fs = self.translate_path(self.path)
        if fs is None:
            self._fail(); self.send_error(403, "forbidden"); return
        ext = os.path.splitext(fs)[1].lower()
        if ext not in AUDIO_EXT or not os.path.isfile(fs):
            self._fail(); self.send_error(404, "not found"); return
        rel_parts = os.path.relpath(fs, self.root).split(os.sep)
        if any(p.startswith(".") for p in rel_parts):
            self._fail(); self.send_error(403, "hidden"); return
        try:
            with open(fs, "rb") as f:
                data = f.read()
            self.send_response(200)
            self.send_header("Content-Type", AUDIO_EXT[ext])
            self.send_header("Content-Length", str(len(data)))
            self.send_header("Accept-Ranges", "bytes")
            self.end_headers()
            self.wfile.write(data)
        except (BrokenPipeError, ConnectionResetError):
            pass

    do_HEAD = do_GET


class ThreadingServer(socketserver.ThreadingMixIn, socketserver.TCPServer):
    daemon_threads = True
    allow_reuse_address = True


def main():
    ap = argparse.ArgumentParser(description="HTTP-зеркало Music/ для мода «унесённые»")
    ap.add_argument("--root", required=True, help="путь к папке Music (в Profile сервера)")
    ap.add_argument("--port", type=int, default=8788)
    ap.add_argument("--host", default="0.0.0.0")
    args = ap.parse_args()

    root = os.path.abspath(args.root)
    for sub in ("Type", "CD"):
        os.makedirs(os.path.join(root, sub), exist_ok=True)
    radio = os.path.join(root, "Radio.txt")
    if not os.path.isfile(radio):
        with open(radio, "w", encoding="utf-8") as f:
            f.write("; Название = URL\n"
                    "Апекс = http://62.152.59.3:8000/nkz\n"
                    "Европа+ = http://online-2.gkvr.ru:8000/europa_nkz_64.aac\n"
                    "ЮморFM = http://62.231.184.253:8000/humor\n")
    MusicHandler.root = root
    print("[унесённые] раздам %s на http://%s:%d" % (root, args.host, args.port))
    ThreadingServer((args.host, args.port), MusicHandler).serve_forever()


if __name__ == "__main__":
    main()
