import subprocess
import tempfile
import unittest
import wave
from pathlib import Path

from ai_studio.runtime_automation.capture.backends.windows_process_loopback import (
    ProcessLoopbackError,
    build_capture_command,
    capture_process_audio,
)


class WindowsProcessLoopbackCommandTest(unittest.TestCase):
    def test_builds_stable_include_tree_command(self):
        command = build_capture_command(
            Path("helper.exe"),
            pid=42,
            expected_creation_time_100ns=123456,
            output=Path("capture.wav"),
            duration_seconds=1.251,
        )

        self.assertEqual(
            command,
            [
                "helper.exe",
                "--pid",
                "42",
                "--expected-creation-time-100ns",
                "123456",
                "--include-tree",
                "--output",
                "capture.wav",
                "--duration-ms",
                "1251",
            ],
        )

    def test_rejects_invalid_pid_before_launch(self):
        with self.assertRaisesRegex(ProcessLoopbackError, "positive integer"):
            build_capture_command(
                Path("helper.exe"),
                pid=0,
                expected_creation_time_100ns=123456,
                output=Path("capture.wav"),
                duration_seconds=1,
            )

    def test_rejects_sub_millisecond_duration(self):
        with self.assertRaisesRegex(ProcessLoopbackError, "at least one millisecond"):
            build_capture_command(
                Path("helper.exe"),
                pid=42,
                expected_creation_time_100ns=123456,
                output=Path("capture.wav"),
                duration_seconds=0.0001,
            )

    def test_rejects_nonfinite_and_oversized_duration(self):
        for duration in (float("nan"), float("inf"), "1"):
            with self.subTest(duration=duration):
                with self.assertRaisesRegex(ProcessLoopbackError, "finite number"):
                    build_capture_command(
                        Path("helper.exe"),
                        pid=42,
                        expected_creation_time_100ns=123456,
                        output=Path("capture.wav"),
                        duration_seconds=duration,
                    )
        with self.assertRaisesRegex(ProcessLoopbackError, "six hours"):
            build_capture_command(
                Path("helper.exe"),
                pid=42,
                expected_creation_time_100ns=123456,
                output=Path("capture.wav"),
                duration_seconds=21_600.001,
            )

    def test_rejects_missing_process_identity(self):
        with self.assertRaisesRegex(ProcessLoopbackError, "creation_time"):
            build_capture_command(
                Path("helper.exe"),
                pid=42,
                expected_creation_time_100ns=0,
                output=Path("capture.wav"),
                duration_seconds=1,
            )


class WindowsProcessLoopbackCaptureTest(unittest.TestCase):
    def test_success_requires_nonempty_pcm_wav(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "capture.wav"

            def fake_runner(command, **kwargs):
                staging = Path(command[command.index("--output") + 1])
                with wave.open(str(staging), "wb") as wav:
                    wav.setnchannels(2)
                    wav.setsampwidth(2)
                    wav.setframerate(48_000)
                    wav.writeframes(b"\x01\x00\x01\x00" * 480)
                report = (
                    '{"schema":"ai_studio.windows_process_loopback","version":2,'
                    '"status":"ok","pid":42,'
                    '"targetCreationTime100ns":123456,'
                    '"durationMs":10,"sampleRate":48000,"channels":2,'
                    '"bitsPerSample":16,"dataBytes":1920,'
                    '"discontinuities":0,"timestampErrors":0,"positionGaps":0,'
                    '"devicePositionRegressions":0,'
                    '"qpcDriftPpm":0.0,"sampleFrames":480}\n'
                )
                return subprocess.CompletedProcess(command, 0, report, "")

            result = capture_process_audio(
                Path("helper.exe"),
                pid=42,
                expected_creation_time_100ns=123456,
                output=output,
                duration_seconds=0.01,
                runner=fake_runner,
            )

            self.assertEqual(result["status"], "captured")
            self.assertEqual(result["sampleRate"], 48_000)
            self.assertEqual(result["channels"], 2)
            self.assertEqual(result["sampleFrames"], 480)
            self.assertEqual(result["helperReport"]["status"], "ok")

    def test_zero_exit_without_valid_wav_is_a_missing_track(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "capture.wav"

            def fake_runner(command, **kwargs):
                return subprocess.CompletedProcess(command, 0, '{"status":"ok"}\n', "")

            with self.assertRaisesRegex(ProcessLoopbackError, "AUDIO_TRACK_MISSING"):
                capture_process_audio(
                    Path("helper.exe"),
                    pid=42,
                    expected_creation_time_100ns=123456,
                    output=output,
                    duration_seconds=0.01,
                    runner=fake_runner,
                )

    def test_nonzero_exit_preserves_diagnostic(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "capture.wav"

            def fake_runner(command, **kwargs):
                return subprocess.CompletedProcess(command, 7, "", "activation failed")

            with self.assertRaisesRegex(
                ProcessLoopbackError, "BACKEND_EXITED.*activation failed"
            ):
                capture_process_audio(
                    Path("helper.exe"),
                    pid=42,
                    expected_creation_time_100ns=123456,
                    output=output,
                    duration_seconds=0.01,
                    runner=fake_runner,
                )

    def test_timeout_maps_error_and_removes_partial_staging(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "capture.wav"
            output.write_bytes(b"existing")

            def fake_runner(command, **kwargs):
                Path(command[command.index("--output") + 1]).write_bytes(b"partial")
                raise subprocess.TimeoutExpired(command, kwargs["timeout"])

            with self.assertRaisesRegex(ProcessLoopbackError, "BACKEND_TIMEOUT"):
                capture_process_audio(
                    Path("helper.exe"),
                    pid=42,
                    expected_creation_time_100ns=123456,
                    output=output,
                    duration_seconds=0.01,
                    runner=fake_runner,
                )
            self.assertEqual(output.read_bytes(), b"existing")
            self.assertEqual(list(output.parent.glob("*.partial")), [])

    def test_launch_failure_maps_error_and_preserves_final_path(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "capture.wav"
            output.write_bytes(b"existing")

            def fake_runner(command, **kwargs):
                raise FileNotFoundError("missing helper")

            with self.assertRaisesRegex(ProcessLoopbackError, "BACKEND_UNAVAILABLE"):
                capture_process_audio(
                    Path("helper.exe"),
                    pid=42,
                    expected_creation_time_100ns=123456,
                    output=output,
                    duration_seconds=0.01,
                    runner=fake_runner,
                )
            self.assertEqual(output.read_bytes(), b"existing")

    def test_discontinuity_report_rejects_and_does_not_promote(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "capture.wav"

            def fake_runner(command, **kwargs):
                staging = Path(command[command.index("--output") + 1])
                with wave.open(str(staging), "wb") as wav:
                    wav.setnchannels(2)
                    wav.setsampwidth(2)
                    wav.setframerate(48_000)
                    wav.writeframes(b"\x00\x00\x00\x00" * 480)
                report = (
                    '{"schema":"ai_studio.windows_process_loopback","version":2,'
                    '"status":"ok","pid":42,'
                    '"targetCreationTime100ns":123456,'
                    '"durationMs":10,"sampleRate":48000,"channels":2,'
                    '"bitsPerSample":16,"dataBytes":1920,'
                    '"discontinuities":1,"timestampErrors":0,"positionGaps":0,'
                    '"devicePositionRegressions":0,'
                    '"qpcDriftPpm":0.0,"sampleFrames":480}\n'
                )
                return subprocess.CompletedProcess(command, 0, report, "")

            with self.assertRaisesRegex(ProcessLoopbackError, "UNQUALIFIED"):
                capture_process_audio(
                    Path("helper.exe"),
                    pid=42,
                    expected_creation_time_100ns=123456,
                    output=output,
                    duration_seconds=0.01,
                    runner=fake_runner,
                )
            self.assertFalse(output.exists())

    def test_wrong_wav_format_is_not_promoted(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "capture.wav"

            def fake_runner(command, **kwargs):
                staging = Path(command[command.index("--output") + 1])
                with wave.open(str(staging), "wb") as wav:
                    wav.setnchannels(1)
                    wav.setsampwidth(1)
                    wav.setframerate(44_100)
                    wav.writeframes(b"\x00" * 441)
                return subprocess.CompletedProcess(command, 0, "{}", "")

            with self.assertRaisesRegex(ProcessLoopbackError, "UNQUALIFIED"):
                capture_process_audio(
                    Path("helper.exe"),
                    pid=42,
                    expected_creation_time_100ns=123456,
                    output=output,
                    duration_seconds=0.01,
                    runner=fake_runner,
                )
            self.assertFalse(output.exists())

    def test_report_data_size_mismatch_is_not_promoted(self):
        with tempfile.TemporaryDirectory() as temporary:
            output = Path(temporary) / "capture.wav"

            def fake_runner(command, **kwargs):
                staging = Path(command[command.index("--output") + 1])
                with wave.open(str(staging), "wb") as wav:
                    wav.setnchannels(2)
                    wav.setsampwidth(2)
                    wav.setframerate(48_000)
                    wav.writeframes(b"\x00\x00\x00\x00" * 480)
                report = (
                    '{"schema":"ai_studio.windows_process_loopback","version":2,'
                    '"status":"ok","pid":42,'
                    '"targetCreationTime100ns":123456,'
                    '"durationMs":10,"sampleRate":48000,"channels":2,'
                    '"bitsPerSample":16,"dataBytes":4,'
                    '"discontinuities":0,"timestampErrors":0,"positionGaps":0,'
                    '"devicePositionRegressions":0,"qpcDriftPpm":0.0,'
                    '"sampleFrames":480}\n'
                )
                return subprocess.CompletedProcess(command, 0, report, "")

            with self.assertRaisesRegex(ProcessLoopbackError, "dataBytes"):
                capture_process_audio(
                    Path("helper.exe"),
                    pid=42,
                    expected_creation_time_100ns=123456,
                    output=output,
                    duration_seconds=0.01,
                    runner=fake_runner,
                )
            self.assertFalse(output.exists())

    def test_output_parent_failure_maps_stable_error(self):
        with tempfile.TemporaryDirectory() as temporary:
            parent_file = Path(temporary) / "not-a-directory"
            parent_file.write_text("x", encoding="utf-8")
            output = parent_file / "capture.wav"

            with self.assertRaisesRegex(ProcessLoopbackError, "OUTPUT_WRITE_FAILED"):
                capture_process_audio(
                    Path("helper.exe"),
                    pid=42,
                    expected_creation_time_100ns=123456,
                    output=output,
                    duration_seconds=0.01,
                )


if __name__ == "__main__":
    unittest.main()
