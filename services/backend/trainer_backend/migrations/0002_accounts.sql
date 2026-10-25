-- SPDX-License-Identifier: GPL-3.0-or-later
CREATE TABLE trainer_backend.players (
    id uuid PRIMARY KEY REFERENCES trainer_backend.principals(id),
    revision bigint NOT NULL DEFAULT 1 CHECK(revision>0)
);
CREATE TABLE trainer_backend.external_identities (
    provider text NOT NULL CHECK(provider='fluxer'),
    instance text NOT NULL,
    subject varchar(32) NOT NULL,
    player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
    verified_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(provider,instance,subject),
    UNIQUE(provider,instance,player_id)
);
-- Only creates ordinary players. Never elevates/reactivates a principal.
CREATE FUNCTION trainer_backend.resolve_fluxer_player(instance_url text, provider_subject text)
RETURNS uuid LANGUAGE plpgsql SECURITY DEFINER SET search_path='' AS $$
DECLARE player uuid;
BEGIN
    IF instance_url <> 'https://fluxer.app' OR provider_subject !~ '^[0-9]{1,20}$' THEN
        RAISE EXCEPTION 'invalid_identity';
    END IF;
    PERFORM pg_catalog.pg_advisory_xact_lock(pg_catalog.hashtextextended(instance_url || ':' || provider_subject,1));
    SELECT player_id INTO player FROM trainer_backend.external_identities
        WHERE provider='fluxer' AND instance=instance_url AND subject=provider_subject;
    IF player IS NULL THEN
        player := pg_catalog.gen_random_uuid();
        INSERT INTO trainer_backend.principals(id,kind,state) VALUES(player,'player','active');
        INSERT INTO trainer_backend.players(id) VALUES(player);
        INSERT INTO trainer_backend.external_identities(provider,instance,subject,player_id)
            VALUES('fluxer',instance_url,provider_subject,player);
    END IF;
    RETURN player;
END;
$$;
REVOKE ALL ON FUNCTION trainer_backend.resolve_fluxer_player(text,text) FROM PUBLIC;

CREATE TABLE trainer_backend.login_attempts (
    id uuid PRIMARY KEY,
    operation_id uuid NOT NULL UNIQUE,
    payload_hash char(64) NOT NULL,
    claim_challenge char(43) NOT NULL,
    device_label varchar(64) NOT NULL,
    state_hash char(64) NOT NULL UNIQUE,
    browser_hash char(64),
    status text NOT NULL CHECK(status IN ('created','authorizing','verifying','verified','claimed','failed')),
    player_id uuid REFERENCES trainer_backend.players(id),
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    expires_at timestamptz NOT NULL DEFAULT clock_timestamp()+interval '10 minutes'
);
CREATE INDEX login_attempt_expiry ON trainer_backend.login_attempts(expires_at);
CREATE TABLE trainer_backend.device_sessions (
    id uuid PRIMARY KEY,
    player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
    device_label varchar(64) NOT NULL,
    access_hash char(64) NOT NULL UNIQUE,
    access_expires_at timestamptz NOT NULL,
    revision bigint NOT NULL DEFAULT 1 CHECK(revision>0),
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    expires_at timestamptz NOT NULL DEFAULT clock_timestamp()+interval '30 days',
    revoked_at timestamptz
);
CREATE INDEX sessions_player ON trainer_backend.device_sessions(player_id);
CREATE TABLE trainer_backend.refresh_credentials (
    token_hash char(64) PRIMARY KEY,
    session_id uuid NOT NULL REFERENCES trainer_backend.device_sessions(id),
    consumed_at timestamptz
);
CREATE INDEX refresh_session ON trainer_backend.refresh_credentials(session_id);
