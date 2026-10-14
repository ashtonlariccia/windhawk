"""Input: an interactive todo list, saved to %USERPROFILE%\\todo.json.

    python scripts\\serve-output.py 7301 --repl "python -u examples\\output-box\\todo.py"

Click the box and type:  add <text> | done <n> | rm <n> | clear | help
"""

import json
import os
import pathlib

GREEN, DIM, BOLD, RED, RESET = "\x1b[32m", "\x1b[2m", "\x1b[1m", "\x1b[31m", "\x1b[0m"
FILE = pathlib.Path(os.environ.get("USERPROFILE", ".")) / "todo.json"


def load():
    try:
        return json.loads(FILE.read_text(encoding="utf-8"))
    except (FileNotFoundError, ValueError):
        return []


def save(items):
    FILE.write_text(json.dumps(items, indent=1), encoding="utf-8")


def show(items, message=""):
    # \f clears the box, so the list is always redrawn whole.
    out = ["\f"]
    if not items:
        out.append(f"{DIM}Nothing to do. Type: add <text>{RESET}")
    for i, item in enumerate(items, 1):
        mark = f"{GREEN}✓{RESET}" if item["done"] else f"{DIM}○{RESET}"
        text = f"{DIM}{item['text']}{RESET}" if item["done"] else item["text"]
        out.append(f"{DIM}{i:>2}{RESET} {mark} {text}")
    if message:
        out.append(f"\n{message}")
    print("\n".join(out))
    print(f"{BOLD}todo>{RESET} ", end="", flush=True)


def index(items, arg):
    n = int(arg)  # ValueError if not a number
    if not 1 <= n <= len(items):
        raise ValueError
    return n - 1


items = load()
show(items)
while True:
    try:
        line = input()
    except EOFError:
        break
    command, _, arg = line.strip().partition(" ")
    message = ""
    try:
        if command == "add" and arg:
            items.append({"text": arg.strip(), "done": False})
        elif command == "done":
            i = index(items, arg)
            items[i]["done"] = not items[i]["done"]
        elif command == "rm":
            items.pop(index(items, arg))
        elif command == "clear":
            items = [item for item in items if not item["done"]]
        elif command in ("help", ""):
            message = f"{DIM}add <text> · done <n> · rm <n> · clear (removes done){RESET}"
        else:
            message = f"{RED}unknown command: {command}{RESET}  {DIM}(try help){RESET}"
        save(items)
    except ValueError:
        message = f"{RED}no item {arg!r}{RESET}"
    show(items, message)
