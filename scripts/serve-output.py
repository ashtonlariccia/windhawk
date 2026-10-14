"""Serve a command's output to a Widgets output box, and feed it what you type.

Standard library only (with boxlib.py next to it). Binds 127.0.0.1:<port>;
run it windowless with pythonw (e.g. from the Startup folder or Task
Scheduler at logon).

Snapshot -- every connection runs --show and answers with its output, so the
box refreshes every few seconds. A line typed into the box first runs --input
with the line substituted: {} as-is (so "add milk" becomes two arguments),
{q} quoted as one argument. COLUMNS / LINES say how much fits in the box.

    pythonw scripts\\serve-output.py 7300 --show "todo list" --input "todo {}"
    pythonw scripts\\serve-output.py 7301 --input "{}"          # a mini shell

Notes -- a built-in scratchpad kept in a text file. Typed lines are appended;
"/del N" removes line N, "/clear" empties it.

    pythonw scripts\\serve-output.py 7302 --notes "%USERPROFILE%\\notes.txt"

REPL -- one long-running process; its output streams to the box and typed
lines go to its stdin. It restarts if it exits. Output goes through a pipe,
not a console, so the program must flush: a C program's printf output sits
in a buffer until it does (fflush(stdout), or setvbuf(stdout, NULL, _IONBF,
0) once at the start); for Python use -u (PYTHONUNBUFFERED is set for you).

    pythonw scripts\\serve-output.py 7303 --repl "python -i -u"

Commands run through the shell (cmd.exe), with FORCE_COLOR / CLICOLOR_FORCE
set so colour-aware tools keep their colours. Only output boxes can connect;
see boxlib.py.
"""

import argparse
import os
import pathlib
import subprocess
import threading
import time

from boxlib import DIM, ENV, NO_WINDOW, RESET, Box, run, serve, to_utf8


class OneShot:
    """A snapshot per connection: typed lines are applied, then the box gets
    the result and the connection ends."""

    def __init__(self):
        self.lock = threading.Lock()  # one connection's work at a time
        self.pending = b""

    def apply(self, lines: list[str], box: Box) -> bytes:
        raise NotImplementedError

    def render(self, out: bytes, box: Box) -> bytes:
        raise NotImplementedError

    def handle(self, box: Box):
        lines = box.read_lines()
        with self.lock:
            out = self.pending + self.apply(lines, box)
            self.pending = b""
            payload = self.render(out, box)
        box.answer(payload)
        # A line typed after the first moments reaches a connection that's
        # already been answered. It's applied now and what it printed leads
        # the next snapshot.
        if late := box.drain():
            with self.lock:
                self.pending += self.apply(late, box)


class Snapshot(OneShot):
    def __init__(self, show, input_template):
        super().__init__()
        self.show = show
        self.input = input_template

    def apply(self, lines, box):
        out = b""
        if self.input:
            for line in lines:
                out += f"{DIM}› {line}{RESET}\n".encode()
                out += run(substitute(self.input, line), box)
        return out

    def render(self, out, box):
        if self.show:
            if out and not out.endswith(b"\n\n"):
                out += b"\n"
            out += run(self.show, box)
        # No --show and nothing typed: send nothing, so the box keeps the
        # last command's output instead of going blank on every refresh.
        return out


class Notes(OneShot):
    def __init__(self, path):
        super().__init__()
        self.path = pathlib.Path(os.path.expandvars(path)).expanduser()

    def load(self):
        try:
            return self.path.read_text(encoding="utf-8").splitlines()
        except FileNotFoundError:
            return []

    def save(self, notes):
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text("".join(n + "\n" for n in notes), encoding="utf-8")

    def apply(self, lines, box):
        if not lines:
            return b""
        notes = self.load()
        message = ""
        for line in lines:
            text = line.strip()
            if text == "/clear":
                notes = []
            elif text.startswith("/del"):
                try:
                    notes.pop(int(text[4:]) - 1)
                except (ValueError, IndexError):
                    message += f"\x1b[31mno note {text[4:].strip()}{RESET}\n"
            elif text:
                notes.append(text)
        self.save(notes)
        return message.encode("utf-8")

    def render(self, out, box):
        notes = self.load()
        width = len(str(len(notes)))
        body = "".join(f"{DIM}{i:>{width}}{RESET}  {n}\n" for i, n in enumerate(notes, 1))
        if not notes:
            body = f"{DIM}No notes. Type one below; /del N removes one.{RESET}\n"
        return body.encode("utf-8") + out


class Repl:
    HISTORY = 64 * 1024

    def __init__(self, command):
        self.command = command
        self.history = b""
        self.clients = []
        self.lock = threading.Lock()
        self.process = None
        threading.Thread(target=self.supervise, daemon=True).start()

    def broadcast(self, data):
        with self.lock:
            history = self.history + data
            if len(history) > self.HISTORY:
                # Cut at a line break, not inside a character or an escape.
                history = history[-self.HISTORY :]
                history = history[history.find(b"\n") + 1 :]
            self.history = history
            for client in list(self.clients):
                if not client.send(data):
                    self.clients.remove(client)

    def supervise(self):
        while True:
            self.process = process = subprocess.Popen(
                self.command,
                shell=True,
                stdin=subprocess.PIPE,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                env=ENV,
                creationflags=NO_WINDOW,
                bufsize=0,
            )
            assert process.stdout
            while chunk := process.stdout.read(65536):
                self.broadcast(to_utf8(chunk))
            # Nothing of the helper's own goes to the box -- only the
            # program's output -- so a restart is just the program again.
            process.wait()
            time.sleep(1)

    def handle(self, box: Box):
        with self.lock:
            # The box starts afresh on each connection, so replay the recent
            # output first.
            if not box.send(self.history):
                return
            self.clients.append(box)
        try:
            lines = box.feed(b"")
            while True:
                for line in lines:
                    # Not echoed: the box shows typed lines itself, as a
                    # terminal would.
                    process = self.process
                    if process and process.stdin:
                        try:
                            process.stdin.write(line.encode("utf-8") + b"\n")
                            process.stdin.flush()
                        except OSError:
                            pass
                chunk = box.recv()
                if not chunk:
                    break
                lines = box.feed(chunk)
        finally:
            with self.lock:
                if box in self.clients:
                    self.clients.remove(box)


def substitute(template: str, line: str) -> str:
    return template.replace("{q}", subprocess.list2cmdline([line])).replace("{}", line)


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("port", type=int)
    parser.add_argument("--show", help="command whose output is shown")
    parser.add_argument("--input", help="command run per typed line ({} / {q})")
    parser.add_argument("--notes", metavar="FILE", help="built-in notes in FILE")
    parser.add_argument("--repl", metavar="COMMAND", help="long-running process")
    args = parser.parse_args()

    if args.repl:
        mode = Repl(args.repl)
    elif args.notes:
        mode = Notes(args.notes)
    elif args.show or args.input:
        mode = Snapshot(args.show, args.input)
    else:
        parser.error("give --show / --input, --notes or --repl")

    serve(args.port, mode.handle)


if __name__ == "__main__":
    main()
