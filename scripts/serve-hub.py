"""Several services in one Widgets output box, switched by typing their name.

Standard library only (with boxlib.py next to it). Binds 127.0.0.1:<port>;
run it windowless with pythonw.

    pythonw scripts\\serve-hub.py 7300                 # starts on the menu
    pythonw scripts\\serve-hub.py 7300 --start matrix

In the box, "/" lists the services and "/<name>" switches to one; any other
line goes to the service showing. The box keeps one connection open to the
hub, which hands it to whichever service is showing.

A service is a class in SERVICES below: it draws by calling hub.write(), with
the box's size in hub.cols / hub.rows, and redraw() is called whenever the
box connects again or is resized.
"""

import argparse
import random
import threading

from boxlib import DIM, RESET, Box, serve

CLEAR = "\x1b[0m\x1b[2J"


class Service:
    name = ""
    about = ""

    def __init__(self, hub: "Hub"):
        self.hub = hub

    def start(self):
        """Now showing; draw."""
        self.redraw()

    def stop(self):
        """No longer showing; stop drawing before returning."""

    def redraw(self):
        """The box connected again or changed size: draw it all again."""

    def line(self, text: str):
        """A line typed into the box."""


class Menu(Service):
    name = "menu"
    about = "this list"

    def __init__(self, hub, message=""):
        super().__init__(hub)
        self.message = message

    def redraw(self):
        width = max(len(s.name) for s in SERVICES)
        rows = "".join(
            f"  /{s.name:<{width}}  {DIM}{s.about}{RESET}\n" for s in SERVICES
        )
        message = f"\x1b[31m{self.message}{RESET}\n\n" if self.message else ""
        self.hub.write(f"{CLEAR}{message}{rows}")


class Matrix(Service):
    """Digital rain, after cmatrix. Typing 1-9 sets the speed."""

    name = "matrix"
    about = "digital rain; type 1-9 for speed"

    GLYPHS = "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ@#$%&*+=<>?:;~^"
    FPS = 24
    HEAD = (225, 255, 225)
    BRIGHT = (60, 255, 110)
    DARK = (0, 70, 25)
    LEVELS = 8  # trail shades; fewer colour changes to send and draw

    def __init__(self, hub):
        super().__init__(hub)
        self.speed = 5
        self.stopping = threading.Event()
        self.thread = None
        self.size = (0, 0)
        self.drawn = None  # per cell, the (shade, glyph) the box shows
        self.fresh = True
        self.shades = [
            tuple(round(b + (a - b) * (1 - i / (self.LEVELS - 1))) for a, b in zip(self.BRIGHT, self.DARK))
            for i in range(self.LEVELS)
        ]

    def start(self):
        self.stopping.clear()
        self.thread = threading.Thread(target=self.loop, daemon=True)
        self.thread.start()

    def stop(self):
        self.stopping.set()
        if self.thread:
            self.thread.join()
            self.thread = None

    def redraw(self):
        self.fresh = True  # the box starts afresh; the next frame is whole

    def line(self, text):
        if text.strip() in list("123456789"):
            self.speed = int(text.strip())

    def reset(self, cols, rows):
        self.size = (cols, rows)
        self.drawn = None
        self.glyphs = [[random.choice(self.GLYPHS) for _ in range(cols)] for _ in range(rows)]
        # Per column, the drops falling in it: [head row, rows per frame, length]
        self.drops = [[] for _ in range(cols)]
        for column in self.drops:
            if random.random() < 0.5:
                column.append(self.new_drop(rows, random.uniform(-rows, rows)))

    def new_drop(self, rows, y=-1.0):
        return [y, random.uniform(0.3, 1.0), random.randint(max(3, rows // 3), max(4, rows + 4))]

    def step(self, cols, rows):
        factor = self.speed / 5
        for column in self.drops:
            for drop in column:
                drop[0] += drop[1] * factor
            column[:] = [d for d in column if d[0] - d[2] < rows]
            # A new drop once the last one is well on its way.
            if (not column or column[-1][0] - column[-1][2] > 2) and random.random() < 0.04 * factor:
                column.append(self.new_drop(rows))
        for _ in range(max(1, cols * rows // 40)):
            self.glyphs[random.randrange(rows)][random.randrange(cols)] = random.choice(self.GLYPHS)

    def frame(self, cols, rows) -> str:
        # Shade per cell: -1 dark, 0 the head, 1.. the trail.
        shade = [[-1] * cols for _ in range(rows)]
        for c, column in enumerate(self.drops):
            for head, _, length in column:
                top = int(head)
                for r in range(max(0, top - length + 1), min(rows, top + 1)):
                    d = top - r
                    level = 0 if d == 0 else 1 + (d - 1) * (self.LEVELS - 1) // length
                    if shade[r][c] < 0 or level < shade[r][c]:
                        shade[r][c] = level
                if 0 <= top < rows:
                    self.glyphs[top][c] = random.choice(self.GLYPHS)  # heads flicker
        cells = [
            [(level, " " if level < 0 else self.glyphs[r][c]) for c, level in enumerate(shade[r])]
            for r in range(rows)
        ]
        # Only the cells that changed since the last frame are sent, each run
        # of them after a cursor move; a whole frame when the box is fresh.
        drawn = self.drawn
        # Hiding the cursor tells the box nothing is read: no typing into it.
        out = [] if drawn else ["\x1b[?25l\x1b[0m\x1b[2J"]
        current = None
        for r in range(rows):
            column = -1  # where the box's cursor is on this row, if known
            for c in range(cols):
                cell = cells[r][c]
                if drawn[r][c] == cell if drawn else cell[0] < 0:
                    continue
                if column != c:
                    out.append(f"\x1b[{r + 1};{c + 1}H")
                level = cell[0]
                if level != current:
                    if level < 0:
                        out.append("\x1b[0m")
                    elif level == 0:
                        out.append("\x1b[0;1;38;2;%d;%d;%dm" % self.HEAD)
                    else:
                        out.append("\x1b[0;38;2;%d;%d;%dm" % self.shades[level - 1])
                    current = level
                out.append(cell[1])
                column = c + 1
        self.drawn = cells
        out.append("\x1b[0m")
        # The cursor waits on a line of its own below the rain, so whatever is
        # typed into the box goes there instead of over the last row.
        out.append(f"\x1b[{rows + 1};1H\x1b[J")
        return "".join(out)

    def loop(self):
        while not self.stopping.wait(1 / self.FPS):
            cols, rows = self.hub.cols, self.hub.rows
            if not self.hub.connected():
                continue
            if (cols, rows) != self.size:
                self.reset(cols, rows)
            if self.fresh:
                self.fresh = False
                self.drawn = None
            self.step(cols, rows)
            self.hub.write(self.frame(cols, rows))


SERVICES = [Menu, Matrix]


class Hub:
    def __init__(self, start: str):
        self.lock = threading.Lock()
        self.box: Box | None = None
        self.cols, self.rows = 80, 24
        self.switch_lock = threading.Lock()
        self.service: Service = self.make(start)
        self.service.start()

    def make(self, name: str) -> Service:
        for cls in SERVICES:
            if cls.name == name:
                return cls(self)
        return Menu(self, f"No service called {name}")

    def connected(self) -> bool:
        return self.box is not None

    def write(self, text: str):
        with self.lock:
            if self.box and not self.box.send(text.encode("utf-8")):
                self.box = None

    def switch(self, name: str):
        with self.switch_lock:
            self.service.stop()
            self.service = self.make(name or "menu")
            self.service.start()

    def handle(self, box: Box):
        # One box per port: a new connection (the box reconnecting, or the
        # mod reloading) takes over from the last.
        with self.lock:
            old, self.box = self.box, box
            self.cols, self.rows = box.cols, box.rows
        if old:
            try:
                old.sock.close()
            except OSError:
                pass
        self.service.redraw()
        lines = box.feed(b"")
        while True:
            for line in lines:
                text = line.strip()
                if text.startswith("/"):
                    self.switch(text[1:].strip().lower())
                else:
                    self.service.line(line)
            if (box.cols, box.rows) != (self.cols, self.rows) and self.box is box:
                self.cols, self.rows = box.cols, box.rows
                self.service.redraw()
            chunk = box.recv()
            if not chunk:
                break
            lines = box.feed(chunk)
        with self.lock:
            if self.box is box:
                self.box = None


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument("port", type=int)
    parser.add_argument("--start", default="menu", help="service shown first")
    args = parser.parse_args()
    hub = Hub(args.start)
    serve(args.port, hub.handle)


if __name__ == "__main__":
    main()
