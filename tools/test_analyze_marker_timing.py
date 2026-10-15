#!/usr/bin/env python3
"""Known recordings test the measurement tool, not the hardware transport."""
import array
import math
from pathlib import Path
import tempfile
import unittest

from analyze_marker_timing import analyze


class RecordingAnalysisTests(unittest.TestCase):
    rate, count, first, latency = 8000, 64, 100, 40

    def measure(self, offsets=None, count=None, bus=1, latency=None):
        count = self.count if count is None else count
        samples = array.array('f', [0]) * ((self.first + (count + 6) * math.ceil(self.rate * .125)) * 6)
        for i in range(count):
            start = self.first + self.latency + math.floor(i * self.rate * .125 + .5) + (offsets(i) if offsets else 0)
            for f in range(start, start + 80):
                samples[f * 6 + (bus - 1) * 2] = .2
                samples[f * 6 + (bus - 1) * 2 + 1] = -.2
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / 'reference.f32'
            path.write_bytes(samples.tobytes())
            return analyze(path, self.rate, self.first,
                           self.latency if latency is None else latency,
                           bus, self.count)

    def test_known_latency_on_every_return(self):
        for bus in (1, 2, 3):
            result = self.measure(bus=bus)
            self.assertTrue(result['passed'])
            self.assertEqual(result['mean_error_ms'], 0)

    def test_missing_and_extra_onsets_fail(self):
        for count in (self.count - 1, self.count + 1):
            self.assertFalse(self.measure(count=count)['passed'])

    def test_half_sample_offsets_match_host_rounding(self):
        self.rate, self.count = 44100, 8
        result = self.measure()
        self.assertTrue(result['passed'])
        self.assertEqual(result['mean_error_ms'], 0)
        self.assertEqual(result['jitter_sd_ms'], 0)

    def test_latency_error_is_not_fitted_away(self):
        result = self.measure(latency=0)
        self.assertFalse(result['passed'])
        self.assertEqual(result['mean_error_ms'], 5)

    def test_jitter_and_drift_fail(self):
        self.assertFalse(self.measure(offsets=lambda i: 8 if i % 2 else -8)['passed'])
        self.assertFalse(self.measure(offsets=lambda i: i)['passed'])


if __name__ == '__main__':
    unittest.main()
