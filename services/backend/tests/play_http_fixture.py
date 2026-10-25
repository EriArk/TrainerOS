"""Standalone isolated CI API. Fake provider identity; real PostgreSQL/sessions/HTTP.

Runs only in the network-none backend test container. Never connects to a live DB.
"""
import json
import os
from pathlib import Path
import tempfile
import uvicorn
from conftest import postgres
from test_accounts import CONFIG,Provider,login
from trainer_backend.accounts.service import Accounts
from trainer_backend.db import Database
from trainer_backend.play.registry import register
from trainer_backend.app import create_app
from trainer_backend.config import Settings
from trainer_backend.jobs import Worker
from trainer_backend.play.service import verify_result
import threading
import time

generator=postgres.__wrapped__()
admin,runtime=next(generator)
db=Database(runtime)
stop=threading.Event()
try:
    fixture=Path(os.environ['PLAY_FIXTURE_DIR'])
    rule={'id':'traineros.scorecard-demo.v1','game_id':'org.traineros.scorecard-demo','content_id':'three-segment-demo-v1',
          'definition':{'validator':'scorecard.v1','segments':3,'minimum':1,'maximum':99,'direction':'asc','trust':'peer_confirmed','label':'Isolated SDK integration'}}
    with tempfile.TemporaryDirectory() as directory:
        path=Path(directory)/'rules.json';path.write_text(json.dumps(rule));register(admin,path)
    accounts=Accounts(db,CONFIG,Provider())
    for i in range(5):
        accounts.provider.subject=str(2000+i)
        account=login(accounts)
        path=fixture/f'player-{i}.json'
        path.write_text(json.dumps({'owner':f'isolated-{i}','origin':'http://127.0.0.1:19091','refresh_token':account['refresh_token']}))
        path.chmod(0o600)
    def work():
        worker=Worker(db,{'results.verify':verify_result})
        while not stop.is_set():
            worker.once();stop.wait(0.05)
    worker=threading.Thread(target=work);worker.start()
    uvicorn.run(create_app(Settings(runtime,'test',oauth=CONFIG),provider=Provider()),host='127.0.0.1',port=19091,access_log=False,log_level='error')
finally:
    stop.set()
    if 'worker' in globals():worker.join()
    db.close()
    for path in fixture.glob('player-*.json'):path.unlink()
    try:next(generator)
    except StopIteration:pass
