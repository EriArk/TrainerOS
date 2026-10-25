# SPDX-License-Identifier: GPL-3.0-or-later
from dataclasses import dataclass
from uuid import UUID, uuid4

from .commands import CODE
from .modules import MODULE_NAMES


class LeaseLost(RuntimeError):
    pass


@dataclass(frozen=True)
class Job:
    id: UUID
    kind: str
    resource_id: UUID
    receipt_id: UUID
    attempt: int
    fence: int
    lease_token: UUID


def enqueue(conn, kind, resource_id, receipt_id, max_attempts=3):
    if not CODE.fullmatch(kind) or kind.split('.')[0] not in MODULE_NAMES:
        raise ValueError('unknown_job_owner')
    if not 1 <= max_attempts <= 10:
        raise ValueError('invalid_attempt_budget')
    row = conn.execute('''INSERT INTO trainer_backend.jobs(id,kind,resource_id,receipt_id,max_attempts)
        VALUES (%s,%s,%s,%s,%s) ON CONFLICT(kind,receipt_id,resource_id)
        DO UPDATE SET kind=EXCLUDED.kind RETURNING id''',
        (uuid4(), kind, resource_id, receipt_id, max_attempts)).fetchone()
    return row['id']


def claim(db, kinds, lease_seconds=30):
    if not kinds:
        return None
    if not 1 <= lease_seconds <= 300:
        raise ValueError('lease_out_of_bounds')
    with db.transaction() as conn:
        conn.execute('''UPDATE trainer_backend.jobs SET state='dead_letter',lease_token=NULL,
            lease_until=NULL,error_code='attempts_exhausted' WHERE kind=ANY(%s) AND state='leased'
            AND lease_until<=clock_timestamp() AND attempt>=max_attempts''', (list(kinds),))
        row = conn.execute('''SELECT id FROM trainer_backend.jobs WHERE kind=ANY(%s)
            AND attempt<max_attempts AND ((state='queued' AND available_at<=clock_timestamp())
            OR (state='leased' AND lease_until<=clock_timestamp()))
            ORDER BY available_at,created_at,id FOR UPDATE SKIP LOCKED LIMIT 1''', (list(kinds),)).fetchone()
        if not row:
            return None
        claimed = conn.execute('''UPDATE trainer_backend.jobs SET state='leased',attempt=attempt+1,
            fence=fence+1,lease_token=%s,lease_until=clock_timestamp()+%s*interval '1 second'
            WHERE id=%s RETURNING id,kind,resource_id,receipt_id,attempt,fence,lease_token''',
            (uuid4(), lease_seconds, row['id'])).fetchone()
        return Job(**claimed)


def complete(db, job, handler):
    with db.transaction() as conn:
        row = conn.execute('''SELECT id FROM trainer_backend.jobs WHERE id=%s AND state='leased'
            AND fence=%s AND lease_token=%s AND lease_until>clock_timestamp() FOR UPDATE''',
            (job.id, job.fence, job.lease_token)).fetchone()
        if not row:
            raise LeaseLost('job_lease_lost')
        handler(conn, job)
        changed = conn.execute('''UPDATE trainer_backend.jobs SET state='completed',lease_token=NULL,
            lease_until=NULL,error_code=NULL WHERE id=%s AND fence=%s AND lease_token=%s
            AND lease_until>clock_timestamp()''', (job.id, job.fence, job.lease_token)).rowcount
        if changed != 1:
            raise LeaseLost('job_lease_expired_during_handler')


def fail(db, job, retry_seconds=1):
    if not 0 <= retry_seconds <= 300:
        raise ValueError('retry_out_of_bounds')
    with db.transaction() as conn:
        return conn.execute('''UPDATE trainer_backend.jobs SET
            state=CASE WHEN attempt>=max_attempts THEN 'dead_letter' ELSE 'queued' END,
            available_at=clock_timestamp()+%s*interval '1 second', lease_token=NULL,lease_until=NULL,
            error_code='handler_failed' WHERE id=%s AND state='leased' AND fence=%s AND lease_token=%s
            AND lease_until>clock_timestamp()''',
            (retry_seconds, job.id, job.fence, job.lease_token)).rowcount == 1


class Worker:
    """Trusted handlers only. No import path, command or URL comes from a job."""
    def __init__(self, db, handlers):
        self.db, self.handlers = db, dict(handlers)

    def once(self):
        job = claim(self.db, self.handlers)
        if job is None:
            return 'idle'
        try:
            complete(self.db, job, self.handlers[job.kind])
            return 'completed'
        except LeaseLost:
            return 'lease_lost'
        except Exception:
            fail(self.db, job, min(2 ** job.attempt, 300))
            return 'retry_or_dead_letter'
