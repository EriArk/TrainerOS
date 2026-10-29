-- SPDX-License-Identifier: GPL-3.0-or-later
CREATE TABLE trainer_backend.support_links (
    player_id uuid PRIMARY KEY REFERENCES trainer_backend.players(id),
    subject varchar(32) UNIQUE,
    member_id varchar(128),
    credentials text,
    revision bigint NOT NULL DEFAULT 0 CHECK(revision>=0),
    lease uuid,
    lease_until timestamptz,
    generation bigint NOT NULL DEFAULT 0,
    checked_at timestamptz,
    next_check_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    state varchar(24) NOT NULL DEFAULT 'unlinked' CHECK(state IN ('unlinked','connected','needs_relink','disconnected')),
    CHECK((state='connected')=(credentials IS NOT NULL))
);
CREATE INDEX support_links_due ON trainer_backend.support_links(next_check_at) WHERE state='connected';
CREATE TABLE trainer_backend.support_link_attempts (
    id uuid PRIMARY KEY,
    player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
    session_id uuid NOT NULL REFERENCES trainer_backend.device_sessions(id),
    generation bigint NOT NULL,
    claim_challenge char(43) NOT NULL,
    state_hash char(64) NOT NULL UNIQUE,
    ticket_hash char(64) NOT NULL,
    browser_hash char(64),
    handoff_hash char(64),
    credentials text,
    return_uri varchar(128) NOT NULL,
    status varchar(24) NOT NULL DEFAULT 'created' CHECK(status IN ('created','authorizing','exchanging','verified','claimed','failed')),
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    expires_at timestamptz NOT NULL DEFAULT clock_timestamp()+interval '10 minutes'
);
CREATE INDEX support_attempt_owner ON trainer_backend.support_link_attempts(player_id,created_at);
