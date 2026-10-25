-- SPDX-License-Identifier: GPL-3.0-or-later
-- Common infrastructure only. The 48-entity domain design is not implemented here.
CREATE TABLE trainer_backend.principals (
    id uuid PRIMARY KEY,
    kind text NOT NULL CHECK(kind IN ('player','operator','adapter','service')),
    state text NOT NULL CHECK(state IN ('active','suspended','revoked')),
    created_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
-- Lock the identity for the command without granting runtime permission to activate accounts.
CREATE FUNCTION trainer_backend.assert_principal(principal_id uuid, expected_kind text)
RETURNS boolean LANGUAGE plpgsql SECURITY DEFINER SET search_path = '' AS $$
BEGIN
    PERFORM 1 FROM trainer_backend.principals
        WHERE id=principal_id AND kind=expected_kind AND state='active' FOR SHARE;
    RETURN FOUND;
END;
$$;
REVOKE ALL ON FUNCTION trainer_backend.assert_principal(uuid,text) FROM PUBLIC;

CREATE TABLE trainer_backend.operation_receipts (
    id uuid PRIMARY KEY,
    actor_id uuid NOT NULL REFERENCES trainer_backend.principals(id),
    operation_kind varchar(64) NOT NULL,
    operation_id uuid NOT NULL,
    payload_hash char(64) NOT NULL CHECK(payload_hash ~ '^[a-f0-9]{64}$'),
    state text NOT NULL CHECK(state IN ('pending','completed','rejected')),
    resource_id uuid,
    revision bigint NOT NULL DEFAULT 1 CHECK(revision > 0),
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    UNIQUE(actor_id,operation_kind,operation_id)
);

CREATE TABLE trainer_backend.jobs (
    id uuid PRIMARY KEY,
    kind varchar(64) NOT NULL,
    resource_id uuid NOT NULL,
    receipt_id uuid NOT NULL REFERENCES trainer_backend.operation_receipts(id),
    state text NOT NULL DEFAULT 'queued' CHECK(state IN ('queued','leased','completed','dead_letter')),
    attempt integer NOT NULL DEFAULT 0 CHECK(attempt >= 0),
    max_attempts integer NOT NULL CHECK(max_attempts BETWEEN 1 AND 10),
    fence bigint NOT NULL DEFAULT 0 CHECK(fence >= 0),
    lease_token uuid,
    lease_until timestamptz,
    available_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    error_code varchar(64),
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    UNIQUE(kind,receipt_id,resource_id),
    CHECK(attempt <= max_attempts),
    CHECK((state='leased') = (lease_token IS NOT NULL AND lease_until IS NOT NULL))
);
CREATE INDEX jobs_pending ON trainer_backend.jobs(kind,available_at,created_at) WHERE state IN ('queued','leased');

CREATE TABLE trainer_backend.event_index (
    id uuid PRIMARY KEY,
    producer varchar(64) NOT NULL,
    event_type varchar(64) NOT NULL,
    aggregate_id uuid NOT NULL,
    aggregate_revision bigint NOT NULL CHECK(aggregate_revision > 0),
    payload_hash char(64) NOT NULL CHECK(payload_hash ~ '^[a-f0-9]{64}$'),
    created_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE TABLE trainer_backend.outbox (
    event_id uuid PRIMARY KEY REFERENCES trainer_backend.event_index(id),
    payload jsonb NOT NULL CHECK(jsonb_typeof(payload)='object' AND octet_length(payload::text) <= 65536)
);
CREATE TABLE trainer_backend.consumer_receipts (
    consumer varchar(64) NOT NULL,
    event_id uuid NOT NULL REFERENCES trainer_backend.event_index(id),
    disposition text NOT NULL CHECK(disposition IN ('applied','stale')),
    applied_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(consumer,event_id)
);
CREATE TABLE trainer_backend.consumer_watermarks (
    consumer varchar(64) NOT NULL,
    producer varchar(64) NOT NULL,
    aggregate_id uuid NOT NULL,
    revision bigint NOT NULL CHECK(revision >= 0),
    PRIMARY KEY(consumer,producer,aggregate_id)
);
CREATE TABLE trainer_backend.audit_events (
    id uuid PRIMARY KEY,
    actor_id uuid NOT NULL REFERENCES trainer_backend.principals(id),
    action varchar(64) NOT NULL,
    resource_id uuid NOT NULL,
    created_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
