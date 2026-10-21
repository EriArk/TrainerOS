"""Two independent app clients; messages cross the real secure-channel helper."""
from pathlib import Path
import base64
import sys
import time
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'sdk/python'))
from traineros import Client, HostError

with Client('org.example.records', sys.argv[1]) as host:
    assert host.supports('network.records')
    host.call('activity.register', title='Record test', protocol='test-1')
    print('registered', flush=True)
    channel = None
    received = False
    deadline = time.monotonic() + 40
    while time.monotonic() < deadline:
        event = host.poll(0.5)
        if not event:
            continue
        if event['event'] == 'network.ready':
            channel = event['session']['channel']
            assert event['session']['delivery'] == 'reliable-ordered'
            # A stale/foreign grant must not reach the transport.
            try:
                host.call('network.send', channel='old-grant', data='aGVsbG8=')
                raise AssertionError('Foreign channel accepted')
            except HostError as error:
                assert error.code == 'channel_unavailable'
            if sys.argv[2] == 'host':
                host.call('network.send', channel=channel, data=base64.b64encode(b'input:42').decode())
        elif event['event'] == 'network.data':
            assert event['channel'] == channel
            data = base64.b64decode(event['data'], validate=True)
            assert data == (b'echo:42' if sys.argv[2] == 'host' else b'input:42')
            host.call('network.consume', channel=channel, sequence=event['sequence'])
            received = True
            if sys.argv[2] == 'guest':
                host.call('network.send', channel=channel, data=base64.b64encode(b'echo:42').decode())
            print('received', flush=True)
        elif event['event'] == 'network.ack':
            print('consumed', flush=True)
        elif event['event'] == 'network.closed':
            assert received, event
            print('closed', flush=True)
            break
    else:
        raise AssertionError('Session did not finish')
