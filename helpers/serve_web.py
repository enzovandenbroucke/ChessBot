"""Serve the generated static site locally; the engine runs in the browser."""
import argparse
import functools
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("--port", type=int, default=8080)
args = parser.parse_args()
directory = Path(__file__).resolve().parents[1] / "build" / "web"
if not (directory / "engine.wasm").is_file():
    parser.error("Run build-web.ps1 before starting the preview")

class Handler(SimpleHTTPRequestHandler):
    extensions_map = {**SimpleHTTPRequestHandler.extensions_map, ".wasm": "application/wasm"}
    def end_headers(self):
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()

server = ThreadingHTTPServer(("127.0.0.1", args.port), functools.partial(Handler, directory=str(directory)))
print(f"ChessBot: http://127.0.0.1:{args.port} — Ctrl+C to stop", flush=True)
try:
    server.serve_forever()
except KeyboardInterrupt:
    pass
finally:
    server.server_close()
