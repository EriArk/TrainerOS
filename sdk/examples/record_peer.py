"""An app-side record exchange through ordinary Lobby; this is not a game."""
from pathlib import Path
import argparse
import base64
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'python'))
from traineros import Client, HostError, HostUnavailable

parser = argparse.ArgumentParser()
parser.add_argument('--endpoint')
args = parser.parse_args()
try:
    with Client('org.example.records', args.endpoint) as host:
        if not host.supports('network.records'):
            raise HostUnavailable('The shared record helper is not installed on this host')
        host.call('activity.register', title='Record channel example', protocol='example-1')
        host.call('activity.open')
        print('Organizer: choose Record channel example in the ordinary Lobby. Both players: Ready. Organizer: Start.', flush=True)
        while True:
            event = host.poll(1)
            if not event:
                continue
            kind = event['event']
            if kind in ('host.stopping', 'context.revoked'):
                break
            if kind == 'network.ready':
                session = event['session']
                print('Ready:', session['delivery'], flush=True)
                if session['host']:
                    host.call('network.send', channel=session['channel'], data=base64.b64encode(b'hello').decode())
            elif kind == 'network.data':
                data = base64.b64decode(event['data'], validate=True)
                print('Received:', repr(data), flush=True)
                host.call('network.consume', channel=event['channel'], sequence=event['sequence'])
                if data == b'hello':
                    host.call('network.send', channel=event['channel'], data=base64.b64encode(b'hello back').decode())
            elif kind == 'network.closed':
                print('Channel closed:', event['reason'], 'The app and lobby remain independent.', flush=True)
except (HostUnavailable, HostError) as error:
    print('Integration unavailable:', error)
except KeyboardInterrupt:
    pass
