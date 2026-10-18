#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
"""Read-only catalogue endpoint. No upload API, signing key or binary hosting."""
import argparse
import json
from pathlib import Path
from http.server import BaseHTTPRequestHandler, HTTPServer
from urllib.parse import urlsplit,parse_qs
from catalogue import canonical,decode,delta,digest,MAX_BYTES


def response(directory, revision, checksum):
    # Publication switches one complete signed file atomically. Keep 32 historical
    # snapshots for bounded delta lookup; a stale cursor gets one full snapshot.
    current=decode((directory/'current.json').read_bytes())
    value=current['signed']
    if revision==str(value['revision']) and checksum==digest(value):
        return {'kind':'unchanged','revision':value['revision'],'digest':checksum}
    old=directory/(revision+'.json') if revision.isdigit() and len(revision)<17 else None
    if old and old.is_file():
        previous=decode(old.read_bytes())
        if digest(previous['signed'])==checksum and previous['signed']['revision']<value['revision']:
            change=delta(previous,current)
            if len(canonical(change))<len(canonical(current)): return change
    return {'kind':'snapshot','envelope':current}


def serve(directory,port):
    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            u=urlsplit(self.path)
            if u.path!='/v1/catalogue' or len(self.path)>512:
                self.send_error(404);return
            try:
                q=parse_qs(u.query,keep_blank_values=True,max_num_fields=4)
                body=canonical(response(directory,q.get('revision',['0'])[0],q.get('digest',[''])[0]))
                if len(body)>MAX_BYTES: raise ValueError('Catalogue limit exceeded')
                self.send_response(200); self.send_header('Content-Type','application/json')
                self.send_header('Cache-Control','no-store');self.send_header('Content-Length',str(len(body)))
                self.send_header('X-Content-Type-Options','nosniff');self.end_headers();self.wfile.write(body)
            except (OSError,ValueError,KeyError): self.send_error(503,'Catalogue temporarily unavailable')
        def log_message(self,fmt,*args):
            # No user IDs or query cursor logging is needed by this public service.
            pass
    HTTPServer(('127.0.0.1',port),Handler).serve_forever()


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('directory',type=Path);p.add_argument('--port',type=int,default=19107)
    args=p.parse_args();serve(args.directory,args.port)
