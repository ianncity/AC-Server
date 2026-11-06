"""Start the isolated realm, publishing auth only after world readiness."""

import argparse
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import socket
import subprocess
import time


def listening(port: int) -> bool:
    with socket.socket() as connection:
        connection.settimeout(0.2)
        return connection.connect_ex(("127.0.0.1", port)) == 0


def start(core: Path, runtime: Path, timeout: int, configuration: str = "Release") -> None:
    core = core.resolve(strict=True)
    runtime = runtime.resolve(strict=True)
    if configuration not in ("Release", "RelWithDebInfo"):
        raise ValueError("Unsupported native build configuration")
    binaries = core / "var/advai-msvc143/bin" / configuration
    executable_hashes = {}
    for name in ("worldserver", "authserver"):
        executable = binaries / f"{name}.exe"
        config = runtime / f"configs/{name}.conf"
        if not executable.is_file() or not config.is_file():
            raise ValueError(f"Missing executable or configuration for {name}")
        executable_hashes[name] = hashlib.sha256(executable.read_bytes()).hexdigest()
        text = config.read_text(encoding="utf-8")
        if 'BindIP = "127.0.0.1"' not in text or '"127.0.0.1;13316;advai;' not in text:
            raise ValueError(f"Configuration is not an isolated ADVAI realm: {config}")
    if listening(13724) or listening(18085):
        raise ValueError("An isolated realm port is already occupied")
    processes = {}
    handles = []
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    deadline = time.monotonic() + timeout
    try:
        for name, port in (("worldserver", 18085), ("authserver", 13724)):
            output = (runtime / f"{name}-{stamp}.log").open("wb")
            handles.append(output)
            process = subprocess.Popen(
                [str(binaries / f"{name}.exe"), "-c", str(runtime / f"configs/{name}.conf")],
                cwd=runtime, stdin=subprocess.DEVNULL, stdout=output, stderr=subprocess.STDOUT,
                creationflags=getattr(subprocess, "CREATE_NO_WINDOW", 0),
            )
            processes[name] = process
            (runtime / "processes.json").write_text(json.dumps({
                "started_utc": stamp, "runtime": str(runtime),
                "configuration": configuration, "binaries": str(binaries), "sha256": executable_hashes,
                "pids": {key: value.pid for key, value in processes.items()},
            }, indent=2), encoding="utf-8")
            while not listening(port):
                if process.poll() is not None:
                    raise RuntimeError(f"{name} exited with {process.returncode}; see {output.name}")
                if time.monotonic() >= deadline:
                    raise RuntimeError(f"Timed out waiting for {name}; see {output.name}")
                time.sleep(0.5)
        print(f"Isolated realm ready: world {processes['worldserver'].pid}, auth {processes['authserver'].pid}",
              flush=True)
    except BaseException:
        for process in reversed(list(processes.values())):
            if process.poll() is None:
                process.terminate()
                process.wait(timeout=30)
        raise
    finally:
        for output in handles:
            output.close()


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--core", required=True, type=Path)
    parser.add_argument("--runtime", required=True, type=Path)
    parser.add_argument("--timeout", type=int, default=600)
    parser.add_argument("--configuration", choices=("Release", "RelWithDebInfo"), default="Release")
    arguments = parser.parse_args()
    try:
        start(arguments.core, arguments.runtime, arguments.timeout, arguments.configuration)
    except (ValueError, RuntimeError, OSError) as error:
        parser.exit(1, f"ADVAI STARTUP FAILURE: {error}\n")
