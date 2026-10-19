#!/usr/bin/env python3
"""Local, explicit-start microphone recorder. Python standard library only."""
from __future__ import annotations

import argparse
from datetime import datetime
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import io
import json
from pathlib import Path
import re
import threading
from urllib.parse import parse_qs, urlsplit
import wave
import webbrowser

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
MAX_BYTES = 16 * 1024 * 1024
NAME = re.compile(r'[a-z][a-z0-9_-]{0,63}\Z')


class TakeStore:
    def __init__(self, folder: Path):
        self.folder = folder
        self.lock = threading.Lock()

    def save(self, cue: str, note: str, data: bytes) -> dict:
        if not NAME.fullmatch(cue):
            raise ValueError('Choose a cue name using letters, numbers, dashes or underscores.')
        if len(note) > 400 or not 44 < len(data) <= MAX_BYTES:
            raise ValueError('Take is empty or too large, or the note is too long.')
        try:
            with wave.open(io.BytesIO(data), 'rb') as audio:
                channels, width, rate, frames, compression, _ = audio.getparams()
                if channels != 1 or width != 2 or not 8000 <= rate <= 96000 or compression != 'NONE':
                    raise ValueError('Expected a mono 16-bit PCM WAV.')
                if frames <= 0 or frames / rate > 61:
                    raise ValueError('A take must be between zero and sixty seconds.')
                if len(audio.readframes(frames)) != frames * channels * width:
                    raise ValueError('The WAV data is incomplete.')
        except (wave.Error, EOFError) as error:
            raise ValueError('Could not read this WAV take.') from error
        day = datetime.now().astimezone().strftime('%Y-%m-%d')
        folder = self.folder / day
        with self.lock:
            folder.mkdir(parents=True, exist_ok=True)
            if not folder.resolve().is_relative_to(self.folder.resolve()):
                raise ValueError('The take folder must stay inside the recording folder.')
            number = 1
            while True:
                path = folder / f'S_{cue}_take{number:03d}.wav'
                if path.with_suffix('.json').exists() or path.with_suffix('.json').is_symlink():
                    number += 1
                    continue
                created = False
                try:
                    with path.open('xb') as target:
                        created = True
                        target.write(data)
                    break
                except FileExistsError:
                    number += 1
                except OSError:
                    if created:
                        path.unlink(missing_ok=True)
                    raise
            info = {'file': str(path.relative_to(self.folder)), 'cue': cue, 'note': note,
                    'duration': round(frames / rate, 3), 'sample_rate': rate,
                    'channels': channels, 'bit_depth': 16,
                    'recorded_at': datetime.now().astimezone().isoformat(timespec='seconds')}
            # A sidecar failure must not turn a successfully stored WAV into an apparent
            # failed save (which causes duplicate retries and hides the valid recording).
            try:
                with path.with_suffix('.json').open('x') as metadata:
                    metadata.write(json.dumps(info, indent=2) + '\n')
            except OSError:
                info['warning'] = 'The WAV was saved, but its note could not be written. Keep the note separately.'
            return info

    def recent(self) -> list:
        result = []
        candidates = []
        for path in self.folder.glob('*/*.wav'):
            try:
                if path.resolve().is_relative_to(self.folder.resolve()):
                    candidates.append((path.stat().st_mtime_ns, path))
            except OSError:
                continue
        for _, path in sorted(candidates, reverse=True)[:100]:
            try:
                info = json.loads(path.with_suffix('.json').read_text())
                if not isinstance(info, dict) or not isinstance(info.get('duration'), (int, float)):
                    raise ValueError('Invalid metadata')
            except (OSError, ValueError):
                try:
                    with wave.open(str(path), 'rb') as audio:
                        duration = audio.getnframes() / audio.getframerate()
                    info = {'file': str(path.relative_to(self.folder)), 'duration': duration,
                            'note': 'Metadata unavailable; WAV preserved'}
                except (OSError, wave.Error, EOFError, ZeroDivisionError):
                    continue
            result.append(info)
        return result


def make_handler(store: TakeStore):
    class Handler(BaseHTTPRequestHandler):
        def log_message(self, *args):
            pass

        def allowed(self):
            port = self.server.server_port
            hosts = {f'localhost:{port}', f'127.0.0.1:{port}'}
            return self.headers.get('Host') in hosts and self.headers.get('Origin') in ({None} | {f'http://{h}' for h in hosts})

        def reply(self, status, body, mime='application/json'):
            content = json.dumps(body).encode() if mime == 'application/json' else body
            self.send_response(status)
            self.send_header('Content-Type', mime)
            self.send_header('Content-Length', str(len(content)))
            self.send_header('Cache-Control', 'no-store')
            self.send_header('X-Content-Type-Options', 'nosniff')
            self.end_headers()
            self.wfile.write(content)

        def do_GET(self):
            if not self.allowed():
                self.reply(403, {'error': 'Open the recorder from its local address.'}); return
            path = urlsplit(self.path).path
            if path == '/api/takes':
                self.reply(200, {'takes': store.recent(), 'folder': str(store.folder)}); return
            static = {'/': ('index.html', 'text/html; charset=utf-8'),
                      '/app.js': ('app.js', 'text/javascript'),
                      '/capture.js': ('capture.js', 'text/javascript'),
                      '/wav.js': ('wav.js', 'text/javascript'),
                      '/style.css': ('style.css', 'text/css')}
            if path not in static:
                self.reply(404, {'error': 'Not found'}); return
            filename, mime = static[path]
            self.reply(200, (HERE / filename).read_bytes(), mime)

        def do_POST(self):
            if not self.allowed():
                self.reply(403, {'error': 'Only the local recorder can save takes.'}); return
            url = urlsplit(self.path)
            if url.path != '/api/takes':
                self.reply(404, {'error': 'Not found'}); return
            try:
                size = int(self.headers.get('Content-Length', '0'))
                if not 44 < size <= MAX_BYTES:
                    raise ValueError('Take is empty or too large.')
                query = parse_qs(url.query)
                data = self.rfile.read(size)
                if len(data) != size:
                    raise ValueError('The uploaded take is incomplete.')
                result = store.save(query.get('cue', [''])[0], query.get('note', [''])[0], data)
                self.reply(201, result)
            except ValueError as error:
                self.reply(400, {'error': str(error)})
            except OSError:
                self.reply(500, {'error': 'Could not write the take. Your recording is still available to download.'})
    return Handler


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=0)
    parser.add_argument('--output', type=Path, default=ROOT / 'Tallow/SourceAssets/Audio/RecordedTakes')
    parser.add_argument('--open', action='store_true')
    args = parser.parse_args()
    server = ThreadingHTTPServer(('127.0.0.1', args.port), make_handler(TakeStore(args.output.resolve())))
    url = f'http://127.0.0.1:{server.server_port}/'
    print(f'TALLOW recorder: {url}\nTakes: {args.output.resolve()}', flush=True)
    if args.open:
        webbrowser.open(url)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
