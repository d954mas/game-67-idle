"""Check built container boundaries and codec-neutral music IDs."""
import argparse
from pathlib import Path
import struct
import subprocess
import tempfile


def entries(path):
    data = path.read_bytes()
    _, _, version, count, *_ = struct.unpack_from("<IIHHIIIII", data)
    assert version == 3
    return [(entry[0], data[entry[1]:entry[1] + entry[2]]) for i in range(count)
            for entry in [struct.unpack_from("<QIIHBBI", data, 32 + i * 24)]]


def run(builder):
    with tempfile.TemporaryDirectory(prefix="music-pack-") as directory:
        root = Path(directory)
        pack, headers = root / "packs", root / "headers"
        pack.mkdir()
        headers.mkdir()
        subprocess.run([str(builder), str(pack), str(headers)], cwd=root, check=True, capture_output=True)
        assert not (pack / "music.ntpack").exists()
        source = root / "assets/audio/music"
        (source / "quiet place").mkdir(parents=True)
        fixtures = {"a.mp3": b"short encoded fixture", "quiet place/theme.mp3": b"different encoded fixture"}
        for name, payload in fixtures.items():
            (source / name).write_bytes(payload)
        subprocess.run([str(builder), str(pack), str(headers),
                        *["assets/audio/music/" + name for name in fixtures]], cwd=root, check=True, capture_output=True)
        core = entries(pack / "game.ntpack")
        music = entries(pack / "music.ntpack")
        assert len(music) == len(fixtures)
        assert not {entry[0] for entry in core} & {entry[0] for entry in music}
        header = (headers / "music.h").read_text()
        for name, payload in fixtures.items():
            assert f"audio/music/{name[:-4]} */" in header
            assert any(blob.endswith(payload) for _, blob in music)
            assert not any(blob.endswith(payload) for _, blob in core)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(__doc__)
    parser.add_argument("--builder", required=True, type=Path)
    run(parser.parse_args().builder.resolve())
