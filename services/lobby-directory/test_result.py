import json
import unittest
import uuid
from result import command_result, PREFIX


class CommandResultTest(unittest.TestCase):
    def command(self):
        return dict(v=2, product='abyssme', protocol=1, request=str(uuid.uuid4()),
                    lobby=str(uuid.uuid4()), revision=1, op='upsert', secret='never reflect')

    def test_correlated_whitelisted_reply(self):
        command = self.command()
        reply = command_result('123', command, 'already-listed', 1000)
        value = json.loads(reply[len(PREFIX):-4])
        self.assertEqual(value['request'], command['request'])
        self.assertEqual(value['result'], 'already-listed')
        self.assertEqual(value['owner'], '123')
        self.assertEqual(value['expires'], 1090)
        self.assertNotIn('secret', value)

    def test_legacy_has_no_new_response(self):
        command = self.command()
        command['v'] = 1
        self.assertIsNone(command_result('123', command, 'accepted', 1000))

    def test_invalid_correlation_is_not_reflected(self):
        for key, value in [('request', 'arbitrary text'), ('lobby', str(uuid.UUID(int=0))),
                           ('revision', True), ('op', 'other'), ('product', 'traineros'),
                           ('protocol', True), ('protocol', 1.0), ('v', 2.0)]:
            command = self.command()
            command[key] = value
            self.assertIsNone(command_result('123', command, 'accepted', 1000))

    def test_unknown_result_is_not_reflected(self):
        self.assertIsNone(command_result('123', self.command(), 'private diagnostic', 1000))
