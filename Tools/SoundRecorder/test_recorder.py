import io
import os
import http.client
import socket
import threading
from http.server import ThreadingHTTPServer
from unittest.mock import patch
from datetime import datetime
from pathlib import Path
import tempfile
import unittest
import wave
from run import TakeStore, make_handler


def sample():
    data = io.BytesIO()
    with wave.open(data, 'wb') as audio:
        audio.setparams((1, 2, 48000, 0, 'NONE', 'not compressed'))
        audio.writeframes(b'\x00\x00' * 480)
    return data.getvalue()


class TakeTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.store = TakeStore(Path(self.temp.name))

    def tearDown(self):
        self.temp.cleanup()

    def test_preserves_takes_and_metadata(self):
        first = self.store.save('match-strike', 'dry wood', sample())
        second = self.store.save('match-strike', 'softer', sample())
        self.assertNotEqual(first['file'], second['file'])
        self.assertEqual((self.store.folder / first['file']).read_bytes(), sample())
        self.assertEqual(first['duration'], .01)
        self.assertEqual(len(self.store.recent()), 2)

    def test_rejects_invalid_names_and_audio(self):
        for name in ['../escape', '/absolute', '', 'a/b', 'a' * 65]:
            with self.assertRaises(ValueError): self.store.save(name, '', sample())
        with self.assertRaises(ValueError): self.store.save('door', '', b'x' * 100)
        with self.assertRaises(ValueError): self.store.save('door', '', sample()[:-4])
        self.assertEqual(list(self.store.folder.iterdir()), [])

    def test_metadata_failure_preserves_wav_and_recent_list(self):
        original = Path.open
        def open_file(path, *args, **kwargs):
            if path.suffix == '.json' and args and args[0] == 'x':
                raise OSError('metadata write failed')
            return original(path, *args, **kwargs)
        with patch.object(Path, 'open', open_file):
            take = self.store.save('chair', 'keep this note', sample())
        self.assertIn('warning', take)
        self.assertEqual((self.store.folder / take['file']).read_bytes(), sample())
        self.assertEqual(self.store.recent()[0]['file'], take['file'])

    def test_orphan_metadata_is_not_overwritten_and_recent_is_chronological(self):
        first = self.store.save('zz-old', '', sample())
        second = self.store.save('aa-new', '', sample())
        os.utime(self.store.folder / first['file'], ns=(1, 1))
        self.assertEqual(self.store.recent()[0]['file'], second['file'])
        orphan = (self.store.folder / first['file']).with_name('S_chair_take001.json')
        orphan.write_text('preserve this metadata')
        take = self.store.save('chair', '', sample())
        self.assertTrue(take['file'].endswith('take002.wav'))
        self.assertEqual(orphan.read_text(), 'preserve this metadata')

    def test_day_symlink_cannot_escape_output(self):
        with tempfile.TemporaryDirectory() as outside:
            day = datetime.now().astimezone().strftime('%Y-%m-%d')
            (self.store.folder / day).symlink_to(outside, target_is_directory=True)
            with self.assertRaises(ValueError): self.store.save('chair', '', sample())
            self.assertEqual(list(Path(outside).iterdir()), [])

    def test_failed_wav_write_removes_partial_file(self):
        original = Path.open
        class FailingWriter:
            def __init__(self, target): self.target = target
            def __enter__(self): return self
            def __exit__(self, *args): self.target.close()
            def write(self, data):
                self.target.write(data[:8])
                raise OSError('disk full')
        def open_file(path, *args, **kwargs):
            target = original(path, *args, **kwargs)
            return FailingWriter(target) if path.suffix == '.wav' and args and args[0] == 'xb' else target
        with patch.object(Path, 'open', open_file):
            with self.assertRaises(OSError): self.store.save('chair', '', sample())
        self.assertEqual(list(self.store.folder.glob('*/*.wav')), [])

    def test_http_rejects_short_upload_even_if_wav_itself_is_complete(self):
        server = ThreadingHTTPServer(('127.0.0.1', 0), make_handler(self.store))
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        conn = http.client.HTTPConnection('127.0.0.1', server.server_port, timeout=3)
        try:
            data = sample()
            conn.putrequest('POST', '/api/takes?cue=chair')
            conn.putheader('Content-Length', str(len(data) + 10))
            conn.endheaders(data)
            conn.sock.shutdown(socket.SHUT_WR)
            response = conn.getresponse()
            self.assertEqual(response.status, 400)
            self.assertIn(b'incomplete', response.read())
            self.assertEqual(list(self.store.folder.glob('*/*.wav')), [])
        finally:
            conn.close(); server.shutdown(); server.server_close(); worker.join()


if __name__ == '__main__':
    unittest.main()
