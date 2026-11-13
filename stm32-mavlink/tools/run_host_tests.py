"""Build/run the native bridge host checks; requires GCC, no Python packages."""
from pathlib import Path
import os
import shutil
import subprocess


def main():
    base = Path(__file__).resolve().parents[1]
    compiler = shutil.which(os.environ.get("CC", "gcc"))
    if not compiler:
        raise SystemExit("GCC not found; add MinGW to PATH or set CC")
    env = os.environ.copy()
    env["PATH"] = str(Path(compiler).parent) + os.pathsep + env["PATH"]
    output = base / "build/host-native"
    output.mkdir(parents=True, exist_ok=True)
    flags = ["-std=c11", "-Wall", "-Wextra", "-Werror"]
    for folder in ["common", "vendor_protocol", "mavlink_handler", "status_reporter"]:
        flags.append("-I" + str(base / "App" / folder))
    flags += ["-isystem", str(base / "ThirdParty/mavlink")]
    vendor = "App/vendor_protocol/vendor_protocol.c"
    tests = {
        "test_stage1": ["test_stage1.c", "App/common/ring_buffer.c"],
        "test_vendor": ["test_vendor_protocol.c", vendor],
        "test_bench_status": ["test_bench_status.c", vendor],
        "test_status_reporter": ["test_status_reporter.c", "App/status_reporter/status_reporter.c", vendor],
    }
    native = ["test_mavlink_bridge.c", "App/mavlink_handler/mavlink_handler.c", vendor]
    tests["test_native_defaults"] = native
    for yaw, pitch in [(1, 1), (-1, 1), (1, -1), (-1, -1)]:
        tests[f"test_native_y{yaw}_p{pitch}"] = native
    for name, sources in tests.items():
        executable = output / (name + ".exe")
        command = [compiler, *flags]
        if name.startswith("test_native_y"):
            yaw, pitch = name.removeprefix("test_native_y").split("_p")
            command += [f"-DGIMBAL_YAW_SIGN={yaw}", f"-DGIMBAL_PITCH_SIGN={pitch}"]
        command += ["-o", str(executable), *sources, "-lm"]
        subprocess.run(command, cwd=base, env=env, check=True)
        args = [str(executable)]
        if name.startswith("test_native_"):
            args.append(str(base / "tests/data/telem2_mount_rx.hex"))
        subprocess.run(args, cwd=base, env=env, check=True)
    print("PASS: four support suites + native defaults and all four sign combinations")


if __name__ == "__main__":
    main()
