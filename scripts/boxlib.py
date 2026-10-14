"""What the Widgets output-box services share: the box's side of the
connection, and running commands for it.

Every connection from a box opens with a size line, ESC "size COLS ROWS"
newline, and the box sends another whenever a resize changes either. Lines
that start with ESC are the box's; everything else is what was typed.

A connection that doesn't open with the size line isn't a box and is dropped.
That matters: a web page can POST to any localhost port, and without the
check every line of the request would be taken as typed input -- commands run,
text fed to a REPL. A browser's first line is always "METHOD /path HTTP/1.1".
"""

import os
import socket
import subprocess
import sys

DIM = "\x1b[2m"
RESET = "\x1b[0m"

ENV = dict(
    os.environ,
    FORCE_COLOR="1",
    CLICOLOR_FORCE="1",
    PYTHONIOENCODING="utf-8",
    PYTHONUTF8="1",
    PYTHONUNBUFFERED="1",
)
NO_WINDOW = getattr(subprocess, "CREATE_NO_WINDOW", 0)

SIZE = b"\x1bsize "


class Box:
    """One connection from an output box."""

    def __init__(self, sock: socket.socket):
        self.sock = sock
        self.cols = 80
        self.rows = 24
        self.buffer = b""

    def handshake(self, timeout: float = 2.0) -> bool:
        """Waits for the size line the box opens with."""
        self.sock.settimeout(timeout)
        try:
            while b"\n" not in self.buffer and len(self.buffer) < 64:
                chunk = self.sock.recv(64)
                if not chunk:
                    return False
                self.buffer += chunk
        except OSError:
            return False
        if not self.buffer.startswith(SIZE):
            return False
        line, self.buffer = self.buffer.split(b"\n", 1)
        return self.control(line)

    def control(self, line: bytes) -> bool:
        if line.startswith(SIZE):
            try:
                cols, rows = (int(v) for v in line[len(SIZE) :].split())
            except ValueError:
                return False
            self.cols, self.rows = max(1, cols), max(1, rows)
            return True
        return False  # an ESC line from a newer box: ignored

    def feed(self, data: bytes) -> list[str]:
        """Typed lines in `data`; the box's own lines are applied, not returned."""
        self.buffer += data
        lines = []
        while b"\n" in self.buffer:
            line, self.buffer = self.buffer.split(b"\n", 1)
            line = line.rstrip(b"\r")
            if line.startswith(b"\x1b"):
                self.control(line)
            else:
                lines.append(line.decode("utf-8", "replace"))
        return lines

    def recv(self) -> bytes:
        """Blocking; b"" once the box is gone."""
        self.sock.settimeout(None)
        try:
            return self.sock.recv(65536)
        except OSError:
            return b""

    def read_lines(self, wait: float = 0.15) -> list[str]:
        """Lines the box sent with this connection. It sends them right after
        the size line, so a short wait is enough; a plain refresh sends none."""
        lines = self.feed(b"")
        self.sock.settimeout(wait)
        try:
            while True:
                chunk = self.sock.recv(65536)
                if not chunk:
                    break
                lines += self.feed(chunk)
                if not self.buffer:
                    self.sock.settimeout(0.03)  # anything else of a paste
        except OSError:
            pass
        return lines

    def answer(self, payload: bytes) -> None:
        """Sends a snapshot and ends this side of the connection."""
        try:
            self.sock.settimeout(10)
            if payload:
                self.sock.sendall(payload)
            self.sock.shutdown(socket.SHUT_WR)
        except OSError:
            pass

    def drain(self, timeout: float = 10) -> list[str]:
        """After answer(): whatever was typed while the snapshot was being
        made, until the box closes its side. Reading it all also keeps the
        close graceful -- closing on unread data resets the connection, and
        a reset can cost the box the snapshot it hasn't read yet."""
        lines = []
        self.sock.settimeout(timeout)
        try:
            while chunk := self.sock.recv(65536):
                lines += self.feed(chunk)
        except OSError:
            pass
        return lines

    def send(self, data: bytes) -> bool:
        try:
            self.sock.sendall(data)
            return True
        except OSError:
            return False


def to_utf8(data: bytes) -> bytes:
    """Console tools write UTF-8 or the OEM code page; the box wants UTF-8."""
    try:
        data.decode("utf-8")
        return data
    except UnicodeDecodeError:
        codec = "oem" if sys.platform == "win32" else "latin-1"
        return data.decode(codec, "replace").encode("utf-8")


def kill_tree(process: subprocess.Popen) -> None:
    # With shell=True the process is cmd.exe; killing only it would leave the
    # program holding the output pipe open, and reading it would never end.
    if sys.platform == "win32":
        subprocess.run(
            ["taskkill", "/T", "/F", "/PID", str(process.pid)],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            creationflags=NO_WINDOW,
        )
    else:
        process.kill()


def run(command: str, box: Box | None = None, timeout: float = 30) -> bytes:
    """Runs a command through the shell and returns its output. With a box,
    COLUMNS / LINES tell the command how much fits."""
    env = ENV
    if box:
        env = dict(ENV, COLUMNS=str(box.cols), LINES=str(box.rows))
    process = subprocess.Popen(
        command,
        shell=True,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        env=env,
        creationflags=NO_WINDOW,
    )
    try:
        out, _ = process.communicate(timeout=timeout)
        return to_utf8(out)
    except subprocess.TimeoutExpired:
        kill_tree(process)
        try:
            process.communicate(timeout=5)
        except subprocess.TimeoutExpired:
            pass
        return f"\x1b[31m{command}: timed out after {timeout:g} s{RESET}\n".encode()


def serve(port: int, handle) -> None:
    """Serves handle(box) per connection from a box, one thread each."""
    import socketserver

    class Handler(socketserver.BaseRequestHandler):
        def handle(self):
            box = Box(self.request)
            if box.handshake():
                handle(box)

    socketserver.ThreadingTCPServer.daemon_threads = True
    socketserver.ThreadingTCPServer.allow_reuse_address = False
    with socketserver.ThreadingTCPServer(("127.0.0.1", port), Handler) as server:
        server.serve_forever()
