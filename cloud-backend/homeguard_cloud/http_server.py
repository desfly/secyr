"""Small production HTTP server for the HomeGuard cloud API.

TLS is expected at the deployment ingress/reverse proxy. The listener binds
localhost by default so it is not accidentally exposed without TLS.
"""
from __future__ import annotations
import json
from http.server import BaseHTTPRequestHandler,ThreadingHTTPServer

def serve(*,router,host:str="127.0.0.1",port:int=8080)->None:
    class Handler(BaseHTTPRequestHandler):
        server_version="HomeGuardCloud/1"
        def do_POST(self): self._handle()
        def do_DELETE(self): self._handle()
        def _handle(self):
            try:
                length=int(self.headers.get("Content-Length","0"))
            except ValueError:
                self.send_error(400); return
            if length<0 or length>8192:
                self.send_error(413); return
            body=self.rfile.read(length)
            result=router.handle(method=self.command,path=self.path,headers=dict(self.headers.items()),body=body)
            payload=b"" if result.status==204 else json.dumps(result.body,separators=(",",":")).encode("utf-8")
            self.send_response(result.status)
            self.send_header("Cache-Control","no-store")
            if payload:
                self.send_header("Content-Type","application/json; charset=utf-8")
                self.send_header("Content-Length",str(len(payload)))
            self.end_headers()
            if payload: self.wfile.write(payload)
        def log_message(self,format,*args):
            return
    ThreadingHTTPServer((host,port),Handler).serve_forever()
