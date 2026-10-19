# TALLOW field recorder

Run `python3 Tools/SoundRecorder/run.py --open` from the repository root. This opens a local browser recorder; no packages or cloud account are needed. The server binds only to 127.0.0.1. Close the launcher process when finished.

Choose a cue, press **Record take**, then **Stop recording**. The browser asks for the microphone only after that first explicit recording action. Listen, add an optional note and save. Space starts/stops when you are not typing; Enter saves a reviewed take. Takes stop automatically after 60 seconds. The microphone closes after each take. No audio is uploaded to an external service.

Saved WAVs and sidecar notes go to `Tallow/SourceAssets/Audio/RecordedTakes/YYYY-MM-DD`. Numbering never overwrites a take. A download link keeps the recorded WAV available if saving fails. Capture requests mono, 48 kHz and disabled browser processing; the WAV header records the actual AudioContext rate. PCM is 16-bit. Watch the peak meter; clipping is flagged, never silently normalized. Input hardware and browser settings can still affect capture.

This is the recording tool for the final sound pass. The cue list does not dictate Vasu's puzzle logic or event timing. Record and trim against the final animations and merged lockpick events. The tool does not start recording by itself.

Validation: `python3 -m unittest discover -s Tools/SoundRecorder -p 'test_*.py'` runs seven storage/HTTP tests, including failed writes, retained WAVs when note storage fails, duplicate names, folder escape rejection and truncated uploads. `node Tools/SoundRecorder/test_capture.mjs` checks synthetic worklet tail flushing, the 60-second limit, WAV encoding, microphone cleanup and save/interruption recovery using native JavaScript stubs. Neither command opens a browser or uses a microphone. A real device recording/playback check remains a user action.

If capture stops unexpectedly, already received audio is retained for review and download, with a warning to check the ending. A worklet that does not acknowledge Stop within two seconds is recovered the same way. If only the note fails to save, the valid WAV remains saved and listed; the interface asks you to keep the note separately.
