"""Private, correlated command results. Acceptance is not projection/admission."""
import json
import uuid

PREFIX = 'TrainerOS lobby directory result\n```json\n'


def command_result(author, command, result, now):
    if (type(command.get('v')) is not int or command.get('v') != 2
            or command.get('product') != 'abyssme'
            or type(command.get('protocol')) is not int or command.get('protocol') != 1):
        return None
    try:
        for key in ('request', 'lobby'):
            value = command[key]
            if str(uuid.UUID(value)) != value or uuid.UUID(value).int == 0:
                return None
        revision = command['revision']
        if type(revision) is not int or not 1 <= revision <= 2**53-1:
            return None
        if command['op'] not in ('upsert', 'withdraw', 'close'):
            return None
        if result not in ('accepted', 'withdrawn', 'closed', 'already-listed',
                          'wrong-product', 'forbidden', 'stale', 'invalid'):
            return None
        value = {key: command[key] for key in ('request', 'lobby', 'revision', 'op')}
        value.update(v=2, product='abyssme', protocol=1, owner=author,
                     result=result, expires=now+90)
        return PREFIX + json.dumps(value, separators=(',', ':')) + '\n```'
    except (ValueError, TypeError, KeyError, AttributeError):
        return None
