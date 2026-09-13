"""Compile host tests into a temporary directory; no hardware or downloads."""
import os
from pathlib import Path
import subprocess
import tempfile

root = Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix="m2-host-tests-") as output:
    tests = sorted((root / "tests").glob("*_test.cpp"))
    if not tests:
        raise SystemExit("no tests discovered")
    for source in tests:
        binary = str(Path(output) / source.stem)
        subprocess.run([os.environ.get("CXX", "c++"), "-std=c++11", "-O1", "-g",
                        "-Wall", "-Wextra", "-Werror", "-pedantic",
                        "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                        "-I" + str(root / "src"), str(source), "-o", binary], check=True)
        subprocess.run([binary], check=True)
    print(f"PASS {len(tests)} host test executables; hardware not tested")
