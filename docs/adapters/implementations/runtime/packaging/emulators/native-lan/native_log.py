# SPDX-License-Identifier: GPL-3.0-or-later
"""Bound diagnostic output without limiting the game's own save-file writes."""
import threading


def capture(stream,path,limit=32*1024**2,observe=None):
    output=path.open('wb')
    def drain():
        remaining=limit
        try:
            with stream,output:
                while data:=stream.read1(65536):
                    if observe is not None:
                        observe(data)
                    if remaining:
                        part=data[:remaining]
                        try:
                            output.write(part)
                            output.flush()
                            remaining-=len(part)
                        except OSError:
                            # Diagnostic storage must not block gameplay.
                            remaining=0
        except (OSError,ValueError):
            pass
    thread=threading.Thread(target=drain,daemon=True)
    thread.start()
    return thread
