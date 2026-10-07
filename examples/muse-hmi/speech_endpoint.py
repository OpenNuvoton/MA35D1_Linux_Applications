"""Detect command completion from 20 ms stereo PCM frames on the gateway."""
import struct
import collections
import webrtcvad

class SpeechEndpoint:
    FRAME_BYTES = 320 * 4  # stereo S16_LE at 16 kHz, 20 ms

    def __init__(self, classifier=None, silence_ms=1000, wait_ms=8000):
        self.vad = webrtcvad.Vad(3)
        # Normalize only the detector copy; keep original audio for the note.
        self.classifier = classifier or self._classify
        self.recent = collections.deque(maxlen=max(1, silence_ms // 20))
        self.silence_ms = silence_ms
        self.wait_ms = wait_ms
        self.pending = bytearray()
        self.elapsed = self.run = self.quiet = 0
        self.heard_speech = False
        self.finished = None

    def _classify(self, pcm):
        samples = struct.unpack('<320h', pcm)
        rms = (sum(x*x for x in samples)/320)**0.5
        # Reject the measured amplified room-noise floor before VAD.
        if rms < 3000:
            return False
        normalized = struct.pack('<320h', *(int(x/8) for x in samples))
        return self.vad.is_speech(normalized, 16000)

    def feed(self, stereo):
        if self.finished:
            return self.finished
        self.pending.extend(stereo)
        while len(self.pending) >= self.FRAME_BYTES:
            frame = bytes(self.pending[:self.FRAME_BYTES])
            del self.pending[:self.FRAME_BYTES]
            pairs = list(struct.iter_unpack('<hh', frame))
            channel = max((0, 1), key=lambda c: sum(p[c]*p[c] for p in pairs))
            mono = b''.join(struct.pack('<h', p[channel]) for p in pairs)
            speaking = self.classifier(mono)
            self.recent.append(bool(speaking))
            self.elapsed += 20
            if speaking:
                self.run += 20
                self.quiet = 0
                if self.run >= 200:
                    self.heard_speech = True
            else:
                self.run = 0
                self.quiet += 20
            if (self.heard_speech and len(self.recent)==self.recent.maxlen
                    and sum(self.recent)<=2):
                self.finished = 'complete'
            elif not self.heard_speech and self.elapsed >= self.wait_ms:
                self.finished = 'no_speech'
            if self.finished:
                self.pending.clear()
                return self.finished
        return None
