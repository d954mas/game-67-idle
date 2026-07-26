import json
import os
import subprocess
import sys
import threading
import time
import unittest
from pathlib import Path

from ai_studio.runtime_automation.capture.backends.ffmpeg_process_loopback_spike import (
    FfmpegProcessLoopbackSpikeError,
    build_mux_command,
    build_video_preflight_command,
    build_video_command,
    inspect_master,
    run_owned_command,
)


class FfmpegOwnedProcessTest(unittest.TestCase):
    def test_timeout_terminates_owned_process(self):
        result = run_owned_command(
            [sys.executable, "-c", "import time; time.sleep(10)"],
            deadline_monotonic=time.monotonic() + 0.05,
            _allow_test_executable=True,
        )

        self.assertEqual(result["status"], "timeout")
        self.assertFalse(result["aliveAfterCleanup"])
        self.assertIsNotNone(result["returnCode"])

    def test_timeout_terminates_child_process_tree(self):
        child_program = "import time; time.sleep(10)"
        leader_program = (
            "import subprocess,sys,time;"
            "p=subprocess.Popen([sys.executable,'-c',sys.argv[1]]);"
            "print(p.pid,flush=True);time.sleep(10)"
        )
        started = time.monotonic()
        result = run_owned_command(
            [sys.executable, "-c", leader_program, child_program],
            deadline_monotonic=started + 0.2,
            _allow_test_executable=True,
        )

        self.assertLess(time.monotonic() - started, 5.0)
        child_pid = int(result["stdout"].splitlines()[0])
        time.sleep(0.05)
        with self.assertRaises(OSError):
            os.kill(child_pid, 0)

    def test_cancel_terminates_child_process_tree(self):
        child_program = "import time; time.sleep(10)"
        leader_program = (
            "import subprocess,sys,time;"
            "p=subprocess.Popen([sys.executable,'-c',sys.argv[1]]);"
            "print(p.pid,flush=True);time.sleep(10)"
        )
        cancel_event = threading.Event()
        timer = threading.Timer(0.2, cancel_event.set)
        timer.start()
        started = time.monotonic()
        try:
            result = run_owned_command(
                [sys.executable, "-c", leader_program, child_program],
                deadline_monotonic=started + 10.0,
                cancel_event=cancel_event,
                _allow_test_executable=True,
            )
        finally:
            timer.cancel()

        self.assertEqual(result["status"], "cancelled")
        self.assertLess(time.monotonic() - started, 5.0)
        child_pid = int(result["stdout"].splitlines()[0])
        time.sleep(0.05)
        with self.assertRaises(OSError):
            os.kill(child_pid, 0)

    def test_communicate_exception_still_cleans_owned_process(self):
        holder = {}

        class InterruptOnce:
            def __init__(self, process):
                self._process = process
                self._interrupted = False

            def __getattr__(self, name):
                return getattr(self._process, name)

            def communicate(self, *args, **kwargs):
                if not self._interrupted:
                    self._interrupted = True
                    raise KeyboardInterrupt()
                return self._process.communicate(*args, **kwargs)

        def interrupting_popen(*args, **kwargs):
            wrapped = InterruptOnce(subprocess.Popen(*args, **kwargs))
            holder["pid"] = wrapped.pid
            return wrapped

        with self.assertRaises(KeyboardInterrupt):
            run_owned_command(
                [sys.executable, "-c", "import time; time.sleep(10)"],
                deadline_monotonic=time.monotonic() + 10.0,
                popen_factory=interrupting_popen,
                _allow_test_executable=True,
            )
        time.sleep(0.05)
        with self.assertRaises(OSError):
            os.kill(holder["pid"], 0)

    def test_rejects_non_allowlisted_executable_before_launch(self):
        with self.assertRaisesRegex(
            FfmpegProcessLoopbackSpikeError, "not allowlisted"
        ):
            run_owned_command(
                [sys.executable, "-c", "print('not launched')"],
                deadline_monotonic=time.monotonic() + 1.0,
            )


class FfmpegProcessLoopbackCommandTest(unittest.TestCase):
    def test_video_preflight_uses_same_exact_hwnd_surface(self):
        command = build_video_preflight_command(
            Path("ffmpeg.exe"),
            hwnd=12345,
            output=Path("preflight.png"),
            width=720,
            height=1280,
        )

        self.assertEqual(command[command.index("-f") + 1], "gdigrab")
        self.assertEqual(command[command.index("-i") + 1], "hwnd=0x3039")
        self.assertNotIn("desktop", command)
        self.assertEqual(command[command.index("-frames:v") + 1], "1")
        self.assertIn(
            "scale=720:1280:force_original_aspect_ratio=decrease,"
            "pad=720:1280:(ow-iw)/2:(oh-ih)/2:black",
            command,
        )
        self.assertEqual(command[-1], "preflight.png")

    def test_video_command_uses_exact_hwnd_gdigrab_and_target_canvas(self):
        command = build_video_command(
            Path("ffmpeg.exe"),
            hwnd=12345,
            source_x=100,
            source_y=200,
            source_width=480,
            source_height=854,
            output=Path("video.mkv"),
            duration_seconds=5,
            fps=60,
            width=720,
            height=1280,
        )

        self.assertNotIn("desktop", command)
        self.assertEqual(command[command.index("-f") + 1], "gdigrab")
        self.assertEqual(command[command.index("-framerate") + 1], "60")
        self.assertEqual(command[command.index("-i") + 1], "hwnd=0x3039")
        self.assertNotIn("-offset_x", command)
        self.assertNotIn("-offset_y", command)
        self.assertIn("h264_nvenc", command)
        self.assertIn(
            "setpts=PTS-STARTPTS,fps=60,"
            "scale=720:1280:force_original_aspect_ratio=decrease,"
            "pad=720:1280:(ow-iw)/2:(oh-ih)/2:black",
            command,
        )
        self.assertEqual(command[-1], "video.mkv")

    def test_mux_command_copies_video_and_encodes_lossless_audio(self):
        command = build_mux_command(
            Path("ffmpeg.exe"),
            video=Path("video.mkv"),
            audio=Path("audio.wav"),
            output=Path("master.mkv"),
        )

        self.assertEqual(command.count("-i"), 2)
        self.assertIn("copy", command)
        self.assertIn("flac", command)
        self.assertIn("-shortest", command)
        self.assertEqual(command[-1], "master.mkv")


class FfmpegProcessLoopbackInspectionTest(unittest.TestCase):
    def test_accepts_one_video_and_one_audio_stream(self):
        probe = {
            "streams": [
                {
                    "index": 0,
                    "codec_type": "video",
                    "codec_name": "h264",
                    "width": 720,
                    "height": 1280,
                    "avg_frame_rate": "60/1",
                    "nb_read_frames": "299",
                },
                {
                    "index": 1,
                    "codec_type": "audio",
                    "codec_name": "flac",
                    "sample_rate": "48000",
                    "channels": 2,
                    "nb_read_frames": "52",
                },
            ],
            "format": {"duration": "4.98", "size": "1000"},
        }

        def fake_runner(command, **kwargs):
            return subprocess.CompletedProcess(command, 0, json.dumps(probe), "")

        result = inspect_master(Path("ffprobe.exe"), Path("master.mkv"), runner=fake_runner)

        self.assertEqual(result["status"], "valid")
        self.assertEqual(result["durationSeconds"], 4.98)
        self.assertEqual(result["video"]["width"], 720)
        self.assertEqual(result["audio"]["sampleRate"], 48_000)

    def test_rejects_master_without_audio(self):
        probe = {
            "streams": [
                {
                    "index": 0,
                    "codec_type": "video",
                    "codec_name": "h264",
                    "width": 720,
                    "height": 1280,
                    "avg_frame_rate": "60/1",
                    "nb_read_frames": "300",
                }
            ],
            "format": {"duration": "5", "size": "1000"},
        }

        def fake_runner(command, **kwargs):
            return subprocess.CompletedProcess(command, 0, json.dumps(probe), "")

        with self.assertRaisesRegex(
            FfmpegProcessLoopbackSpikeError, "AV_VALIDATION_FAILED"
        ):
            inspect_master(Path("ffprobe.exe"), Path("master.mkv"), runner=fake_runner)

    def test_rejects_structural_master_with_too_few_decoded_frames(self):
        probe = {
            "streams": [
                {
                    "index": 0,
                    "codec_type": "video",
                    "codec_name": "h264",
                    "width": 720,
                    "height": 1280,
                    "avg_frame_rate": "30/1",
                    "nb_read_frames": "17",
                },
                {
                    "index": 1,
                    "codec_type": "audio",
                    "codec_name": "flac",
                    "sample_rate": "48000",
                    "channels": 2,
                    "nb_read_frames": "52",
                },
            ],
            "format": {"duration": "4.96", "size": "1000"},
        }

        def fake_runner(command, **kwargs):
            return subprocess.CompletedProcess(command, 0, json.dumps(probe), "")

        with self.assertRaisesRegex(
            FfmpegProcessLoopbackSpikeError, "decoded 17 frames"
        ):
            inspect_master(
                Path("ffprobe.exe"),
                Path("master.mkv"),
                runner=fake_runner,
                expected_width=720,
                expected_height=1280,
                expected_fps=30,
                expected_duration_seconds=5,
            )

    def test_rejects_wrong_codecs_even_when_streams_exist(self):
        probe = {
            "streams": [
                {
                    "index": 0,
                    "codec_type": "video",
                    "codec_name": "vp9",
                    "width": 720,
                    "height": 1280,
                    "avg_frame_rate": "30/1",
                    "nb_read_frames": "150",
                },
                {
                    "index": 1,
                    "codec_type": "audio",
                    "codec_name": "mp3",
                    "sample_rate": "48000",
                    "channels": 2,
                    "nb_read_frames": "52",
                },
            ],
            "format": {"duration": "5", "size": "1000"},
        }

        def fake_runner(command, **kwargs):
            return subprocess.CompletedProcess(command, 0, json.dumps(probe), "")

        with self.assertRaisesRegex(FfmpegProcessLoopbackSpikeError, "video codec"):
            inspect_master(
                Path("ffprobe.exe"),
                Path("master.mkv"),
                runner=fake_runner,
                expected_fps=30,
                expected_duration_seconds=5,
            )

    def test_rejects_wrong_rate_and_duration(self):
        probe = {
            "streams": [
                {
                    "index": 0,
                    "codec_type": "video",
                    "codec_name": "h264",
                    "width": 720,
                    "height": 1280,
                    "avg_frame_rate": "1/1",
                    "nb_read_frames": "3000",
                },
                {
                    "index": 1,
                    "codec_type": "audio",
                    "codec_name": "flac",
                    "sample_rate": "48000",
                    "channels": 2,
                    "nb_read_frames": "100",
                },
            ],
            "format": {"duration": "100", "size": "1000"},
        }

        def fake_runner(command, **kwargs):
            return subprocess.CompletedProcess(command, 0, json.dumps(probe), "")

        with self.assertRaisesRegex(
            FfmpegProcessLoopbackSpikeError, "average frame rate"
        ):
            inspect_master(
                Path("ffprobe.exe"),
                Path("master.mkv"),
                runner=fake_runner,
                expected_fps=30,
                expected_duration_seconds=5,
            )

    def test_rejects_wrong_duration_with_correct_rate(self):
        probe = {
            "streams": [
                {
                    "index": 0,
                    "codec_type": "video",
                    "codec_name": "h264",
                    "width": 720,
                    "height": 1280,
                    "avg_frame_rate": "30/1",
                    "nb_read_frames": "3000",
                },
                {
                    "index": 1,
                    "codec_type": "audio",
                    "codec_name": "flac",
                    "sample_rate": "48000",
                    "channels": 2,
                    "nb_read_frames": "100",
                },
            ],
            "format": {"duration": "100", "size": "1000"},
        }

        def fake_runner(command, **kwargs):
            return subprocess.CompletedProcess(command, 0, json.dumps(probe), "")

        with self.assertRaisesRegex(
            FfmpegProcessLoopbackSpikeError, "differs from"
        ):
            inspect_master(
                Path("ffprobe.exe"),
                Path("master.mkv"),
                runner=fake_runner,
                expected_fps=30,
                expected_duration_seconds=5,
            )

    def test_ffprobe_timeout_maps_stable_error(self):
        def fake_runner(command, **kwargs):
            raise subprocess.TimeoutExpired(command, kwargs["timeout"])

        with self.assertRaisesRegex(
            FfmpegProcessLoopbackSpikeError, "exceeded its deadline"
        ):
            inspect_master(
                Path("ffprobe.exe"),
                Path("master.mkv"),
                runner=fake_runner,
                timeout_seconds=0.01,
            )

    def test_ffprobe_launch_failure_maps_stable_error(self):
        def fake_runner(command, **kwargs):
            raise FileNotFoundError("missing ffprobe")

        with self.assertRaisesRegex(
            FfmpegProcessLoopbackSpikeError, "BACKEND_UNAVAILABLE"
        ):
            inspect_master(
                Path("ffprobe.exe"),
                Path("master.mkv"),
                runner=fake_runner,
            )


if __name__ == "__main__":
    unittest.main()
