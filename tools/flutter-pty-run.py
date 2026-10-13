#!/usr/bin/env python3
"""Run `flutter run` inside a controlling pseudo-terminal.

The DSH background-job runner gives a child process pipes for stdio, not a
controlling TTY. Flutter's resident runner only installs its raw-mode key
handler (r / R / q) when stdin is a terminal, so hot reload is unreachable
from a plain background job.

This wrapper allocates a real pty, runs flutter as its session leader, mirrors
all output to a log file, and exposes a control socket so reload/restart can be
triggered the way the tool expects: by writing the key to its stdin.

Usage:
    tools/flutter-pty-run.py [--log PATH] [--sock PATH] [-- flutter args...]

Control:
    tools/flutter-pty-run.py --send r        # hot reload / restart / quit
    tools/flutter-pty-run.py --send R
    tools/flutter-pty-run.py --send q
"""

from __future__ import annotations

import argparse
import fcntl
import os
import pty
import select
import signal
import socket
import struct
import sys
import termios
import time
from pathlib import Path

DEFAULT_LOG = Path("/tmp/mgcs-flutter-run.log")
DEFAULT_SOCK = Path("/tmp/mgcs-flutter-ctl.sock")


def send_control(sock_path: Path, key: str) -> int:
    """Write a single key to the running flutter process's pty via the socket."""
    payload = key.encode()
    if len(payload) != 1:
        print("error: --send takes exactly one character (r, R, q, h, c, d)", file=sys.stderr)
        return 2
    try:
        with socket.socket(socket.AF_UNIX, socket.SOCK_STREAM) as s:
            s.settimeout(5)
            s.connect(str(sock_path))
            s.sendall(payload)
        print(f"sent {key!r} to {sock_path}")
        return 0
    except FileNotFoundError:
        print(f"error: control socket {sock_path} not found (is flutter running?)", file=sys.stderr)
        return 3
    except OSError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 4


def run(log_path: Path, sock_path: Path, argv: list[str]) -> int:
    for stale in (sock_path,):
        if stale.exists():
            stale.unlink()

    log_path.parent.mkdir(parents=True, exist_ok=True)
    log_fd = os.open(log_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o644)

    pid, master_fd = pty.fork()
    if pid == 0:
        # Child: becomes session leader owning the slave pty as its ctty.
        os.environ.setdefault("TERM", "xterm-256color")
        os.execvp(argv[0], argv)
        os._exit(127)

    # Keep the pty in canonical mode with echo; flutter toggles raw mode itself.
    try:
        attrs = termios.tcgetattr(master_fd)
        attrs[3] |= termios.ECHO
        termios.tcsetattr(master_fd, termios.TCSANOW, attrs)
    except termios.error:
        pass

    server = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    server.bind(str(sock_path))
    os.chmod(sock_path, 0o600)
    server.listen(4)
    server.setblocking(False)

    print(f"flutter-pty-run: pid={pid} log={log_path} sock={sock_path}", flush=True)

    stop = False

    def handle_stop(signum, _frame):  # noqa: ANN001
        nonlocal stop
        stop = True

    signal.signal(signal.SIGTERM, handle_stop)
    signal.signal(signal.SIGINT, handle_stop)

    try:
        while not stop:
            try:
                readable, _, _ = select.select([master_fd, server], [], [], 0.5)
            except InterruptedError:
                continue
            except OSError:
                break

            for fd in readable:
                if fd == master_fd:
                    try:
                        data = os.read(master_fd, 65536)
                    except OSError:
                        data = b""
                    if not data:
                        stop = True
                        break
                    os.write(log_fd, data)
                    sys.stdout.write(data.decode(errors="replace"))
                    sys.stdout.flush()
                elif fd == server:
                    try:
                        conn, _ = server.accept()
                    except OSError:
                        continue
                    with conn:
                        key = conn.recv(16)
                    if key:
                        os.write(master_fd, key)
                        os.write(log_fd, f"\n[ctl] sent {key!r}\n".encode())

            # Reap the child if it exited.
            try:
                done, status = os.waitpid(pid, os.WNOHANG)
                if done:
                    code = os.waitstatus_to_exitcode(status)
                    print(f"\nflutter-pty-run: flutter exited with {code}", flush=True)
                    return code
            except ChildProcessError:
                return 0
    finally:
        for cleanup in (master_fd, log_fd):
            try:
                os.close(cleanup)
            except OSError:
                pass
        server.close()
        if sock_path.exists():
            sock_path.unlink()
        try:
            os.kill(pid, signal.SIGTERM)
        except ProcessLookupError:
            pass

    return 0


def main() -> int:
    ap = argparse.ArgumentParser(description="Run flutter inside a controlling pty")
    ap.add_argument("--log", type=Path, default=DEFAULT_LOG)
    ap.add_argument("--sock", type=Path, default=DEFAULT_SOCK)
    ap.add_argument("--send", help="send one key (r/R/q/h/c/d) to the running flutter")
    ap.add_argument("--workdir", type=Path, default=Path.cwd())
    ap.add_argument("flutter_args", nargs=argparse.REMAINDER)
    args = ap.parse_args()

    if args.send:
        return send_control(args.sock, args.send)

    argv = [a for a in args.flutter_args if a != "--"] or ["flutter", "run", "-d", "macos", "--debug"]
    os.chdir(args.workdir)
    return run(args.log, args.sock, argv)


if __name__ == "__main__":
    sys.exit(main())
