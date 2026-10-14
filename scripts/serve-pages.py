"""Serve pages\<port>\ on 127.0.0.1:<port> for every port folder, in one process.

Standard library only. Run it windowless with pythonw:
    pythonw scripts\serve-pages.py
"""

import functools
import http.server
import pathlib
import threading

PAGES = pathlib.Path(__file__).resolve().parent.parent / "pages"


class QuietHandler(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *args):
        pass

    def end_headers(self):
        # Edits show up on the widget menu's Refresh page.
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()


def serve(port, root):
    handler = functools.partial(QuietHandler, directory=str(root))
    http.server.ThreadingHTTPServer(("127.0.0.1", port), handler).serve_forever()


threads = []
for folder in sorted(PAGES.iterdir()):
    if folder.is_dir() and folder.name.isdigit() and 7200 <= int(folder.name) <= 7230:
        thread = threading.Thread(target=serve, args=(int(folder.name), folder), daemon=True)
        thread.start()
        threads.append(thread)

for thread in threads:
    thread.join()
