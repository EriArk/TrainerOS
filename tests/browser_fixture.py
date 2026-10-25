"""Loopback-only browser download fixture. Logs requests, never user sessions.

Run: python3 browser_fixture.py <evidence-directory> [port]
The deterministic response is 12 MiB; /range advertises validators and ranges,
/plain explicitly disables ranges, /changed changes the validator for slices,
/auth needs the fixture cookie, /login redirects to it. /blob creates a Blob.
"""
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path
import json, sys, time, threading, hashlib

root = Path(sys.argv[1]); root.mkdir(parents=True, exist_ok=True)
block = bytes(range(256)) * 256
payload = block * 192
(root / "expected.json").write_text(json.dumps({"bytes": len(payload), "sha256": hashlib.sha256(payload).hexdigest()}))
lock = threading.Lock()

class Handler(BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"
    def log_message(self, *args): pass
    def do_GET(self):
        path = self.path.split("?")[0]
        if path in ("/input", "/frame"):
            if path == "/input":
                body = (Path(__file__).parent / "browser_input_fixture.html").read_text(encoding="utf-8").replace("PORT", str(self.server.server_port)).encode()
            else:
                body = b'<input id="entry"><script>addEventListener("message",()=>entry.focus());entry.oninput=()=>parent.postMessage(entry.value,"*");</script>'
            self.send_response(200); self.send_header("Content-Type", "text/html; charset=utf-8")
            self.send_header("Content-Length", str(len(body))); self.end_headers(); self.wfile.write(body); return
        with lock:
            with (root / "requests.jsonl").open("a") as out:
                out.write(json.dumps({"path": path, "range": self.headers.get("Range"), "if_range": self.headers.get("If-Range"), "if_match": self.headers.get("If-Match"), "fixture_cookie": "browser71=yes" in self.headers.get("Cookie", "")}) + "\n")
        if path == "/login":
            self.send_response(302); self.send_header("Set-Cookie", "browser71=yes; HttpOnly; SameSite=Strict; Path=/")
            self.send_header("Location", "/auth"); self.send_header("Content-Length", "0"); self.end_headers(); return
        if path == "/blob":
            body = b'''<!doctype html><title>Blob fixture</title><button id="download">Download blob</button><script>
            const b=new Blob(['traineros-browser71-blob'],{type:'application/octet-stream'});
            const a=document.createElement('a');a.href=URL.createObjectURL(b);a.download='traineros-browser71-blob.txt';
            document.getElementById('download').onclick=()=>a.click();setTimeout(()=>a.click(),1000);
            </script>'''
            self.send_response(200);self.send_header("Content-Type", "text/html");self.send_header("Content-Length", str(len(body)));self.end_headers();self.wfile.write(body);return
        if path == "/auth" and "browser71=yes" not in self.headers.get("Cookie", ""):
            self.send_response(403);self.send_header("Content-Length", "0");self.end_headers();return
        if path not in ("/range", "/plain", "/changed", "/auth"):
            body=b'<!doctype html><title>TrainerOS browser check</title><h1>Browser check</h1><input placeholder="Type here"><p><a href="/range">Range download</a></p><p><a href="/plain">Single stream</a></p><p><a href="/login">Authenticated download</a></p><p><a href="/blob">Blob</a></p><div style="height:1800px;background:linear-gradient(#b0d4c9,#86a4bf)"></div><p>End of page</p>'
            self.send_response(200);self.send_header("Content-Type", "text/html");self.send_header("Content-Length", str(len(body)));self.end_headers();self.wfile.write(body);return
        start, end = 0, len(payload)-1
        ranged = path != "/plain" and self.headers.get("Range", "").startswith("bytes=")
        if ranged:
            parts=self.headers['Range'][6:].split('-',1);start=int(parts[0]);end=int(parts[1]) if parts[1] else end
        changed=path=="/changed" and ranged
        # A validator mismatch must not splice a newer representation into the
        # original response. Reject slices with a stale validator.
        if changed:
            self.send_response(412);self.send_header("Content-Length", "0");self.end_headers();return
        self.send_response(206 if ranged else 200)
        self.send_header("Content-Type", "application/octet-stream")
        self.send_header("Content-Disposition", 'attachment; filename="traineros-browser71-'+path[1:]+'.bin"')
        self.send_header("Accept-Ranges", "none" if path=="/plain" else "bytes")
        self.send_header("ETag", '"browser71-changed"' if changed else '"browser71-stable"')
        self.send_header("Content-Length", str(end-start+1))
        if ranged:self.send_header("Content-Range", f"bytes {start}-{end}/{len(payload)}")
        self.end_headers()
        try:
            for offset in range(start,end+1,65536):
                self.wfile.write(payload[offset:min(offset+65536,end+1)]);self.wfile.flush();time.sleep(.06)
        except (BrokenPipeError, ConnectionResetError):pass

server=ThreadingHTTPServer(('127.0.0.1',int(sys.argv[2]) if len(sys.argv)>2 else 18771),Handler)
server.serve_forever()
