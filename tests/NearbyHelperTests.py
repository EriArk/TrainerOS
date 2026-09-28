"""No radios required: identity, pipe framing and owned-connection cleanup."""
import importlib.util
from pathlib import Path
import types
import unittest
import uuid
from unittest.mock import patch

spec = importlib.util.spec_from_file_location('nearby', Path(__file__).resolve().parents[1]/'packaging/integrations/nearby-control.py')
n = importlib.util.module_from_spec(spec); spec.loader.exec_module(n)


class NearbyTests(unittest.TestCase):
    def test_only_traineros_identity_is_discovered(self):
        key = str(uuid.uuid4())
        raw = n.MAGIC + uuid.UUID(key).bytes + 'Misty'.encode()
        self.assertEqual(n.decode_peer({'VendorExtension': [b'Other console', raw]}), {'id': key, 'name': 'Misty'})
        for raw in (b'Other console', n.MAGIC, n.MAGIC+uuid.UUID(key).bytes+b'Bad\nname', n.MAGIC+uuid.UUID(key).bytes+b'\xff'):
            self.assertIsNone(n.decode_peer({'VendorExtension': [raw]}))
        for name in ('', 'x'*33, '\x00Trainer'):
            with self.assertRaises(ValueError): n.identity({'id':key,'name':name})

    def test_partial_and_batched_commands_survive_pipe_buffering(self):
        app = object.__new__(n.Nearby); app.buffer=b''; commands=[]; app.command=commands.append
        with patch.object(n.os,'read',side_effect=[b'{"op":"dis',b'connect"}\n{"op":"configure"}\n']):
            self.assertTrue(app.input(None,None)); self.assertEqual(commands,[])
            self.assertTrue(app.input(None,None))
        self.assertEqual(commands,[b'{"op":"disconnect"}',b'{"op":"configure"}'])

    def test_release_deactivates_only_its_own_connection(self):
        app=object.__new__(n.Nearby); app.active='/traineros/active'; calls=[]
        app.target='';app.incoming=False;app.path=None
        app.manager=types.SimpleNamespace(DeactivateConnection=lambda path,**kw:calls.append(path))
        app.d=types.SimpleNamespace(DBusException=RuntimeError)
        app.release();app.release()
        self.assertEqual(calls,['/traineros/active']);self.assertIsNone(app.active)
        self.assertEqual(app.target,'');self.assertFalse(app.ready)


if __name__ == '__main__':unittest.main()
