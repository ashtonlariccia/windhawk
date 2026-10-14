"""Output only: a clock and stopwatch redrawn in place every second.

    python scripts\\serve-output.py 7300 --repl "python -u examples\\output-box\\timer.py"
"""

import time

CYAN, DIM, BOLD, RESET = "\x1b[36m", "\x1b[2m", "\x1b[1m", "\x1b[0m"

start = time.monotonic()
while True:
    elapsed = int(time.monotonic() - start)
    h, m, s = elapsed // 3600, elapsed // 60 % 60, elapsed % 60
    # ESC[H goes back to the top-left and ESC[J clears from there, so each
    # second redraws the same lines instead of adding new ones.
    print(
        f"\x1b[H\x1b[J"
        f"{BOLD}{time.strftime('%H:%M:%S')}{RESET}  {DIM}{time.strftime('%A %d %B')}{RESET}\n"
        f"\n"
        f"{DIM}running for{RESET} {CYAN}{h:02}:{m:02}:{s:02}{RESET}",
        end="",
        flush=True,
    )
    time.sleep(1 - (time.monotonic() - start) % 1)
