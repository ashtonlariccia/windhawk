"""Robustness: shows that nothing you type can take the box or explorer down.

    python scripts\\serve-output.py 7302 --repl "python -u examples\\output-box\\sturdy.py"

Type a number to have it squared. Then try to break things:

  garbage       bad input is caught and reported, the program carries on
  spam          floods 5000 lines at once; the box keeps the last 2000
  crash         raises an uncaught exception: the traceback shows, the
                program dies, and the helper restarts it a second later
  quit          exits normally; the helper restarts it the same way
  Ctrl+C        the box ignores it; nothing is interrupted

In every case the box stays up and stays connected. A program dying is the
helper's to deal with (it restarts it), never the box's: the box only ever
sees a TCP connection, which the helper keeps open.
"""

import sys

YELLOW, RED, DIM, RESET = "\x1b[33m", "\x1b[31m", "\x1b[2m", "\x1b[0m"

print(f"{DIM}sturdy started -- type a number, or: garbage, spam, crash, quit{RESET}")
while True:
    print("> ", end="", flush=True)
    try:
        line = input().strip()
    except EOFError:
        sys.exit(0)

    if line == "crash":
        raise RuntimeError("crashed on purpose")  # uncaught, on purpose
    if line == "quit":
        print("bye")
        sys.exit(0)
    if line == "spam":
        for i in range(5000):
            print(f"{DIM}line {i}{RESET}")
        continue

    try:
        n = float(line)
        print(f"{n:g}² = {YELLOW}{n * n:g}{RESET}")
    except ValueError:
        print(f"{RED}not a number:{RESET} {line!r}")
