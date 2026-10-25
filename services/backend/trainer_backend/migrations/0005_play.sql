-- Immutable registry revisions; operator writes only, runtime reads.
CREATE TABLE trainer_backend.game_rulesets (
 id text PRIMARY KEY, game_id text NOT NULL, content_id text NOT NULL,
 definition jsonb NOT NULL, rules_hash char(64) NOT NULL,
 active boolean NOT NULL DEFAULT true
);
CREATE TABLE trainer_backend.play_grants (
 id uuid PRIMARY KEY, session_id uuid NOT NULL REFERENCES trainer_backend.device_sessions(id),
 player_id uuid NOT NULL REFERENCES trainer_backend.players(id), app_id text NOT NULL,
 ruleset_id text NOT NULL REFERENCES trainer_backend.game_rulesets(id), token_hash char(64) UNIQUE NOT NULL,
 expires_at timestamptz NOT NULL DEFAULT clock_timestamp()+interval '2 hours', revoked boolean NOT NULL DEFAULT false
);
CREATE INDEX play_grants_session ON trainer_backend.play_grants(session_id);
CREATE TABLE trainer_backend.matches (
 id uuid PRIMARY KEY, ruleset_id text NOT NULL REFERENCES trainer_backend.game_rulesets(id),
 creator_id uuid NOT NULL REFERENCES trainer_backend.players(id), mode text NOT NULL CHECK(mode IN ('individual','teams')),
 seats jsonb NOT NULL, state text NOT NULL DEFAULT 'waiting' CHECK(state IN ('waiting','active','confirmed','disputed','cancelled','rejected','voided')),
 revision bigint NOT NULL DEFAULT 1, created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
 started_at timestamptz, deadline timestamptz NOT NULL DEFAULT clock_timestamp()+interval '2 hours'
);
CREATE INDEX matches_owner ON trainer_backend.matches(creator_id,created_at);
CREATE TABLE trainer_backend.match_participants (
 match_id uuid NOT NULL REFERENCES trainer_backend.matches(id), player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
 side integer NOT NULL CHECK(side BETWEEN 0 AND 15), PRIMARY KEY(match_id,player_id)
);
CREATE INDEX match_participant_player ON trainer_backend.match_participants(player_id,match_id);
CREATE TABLE trainer_backend.result_submissions (
 id uuid PRIMARY KEY, match_id uuid NOT NULL REFERENCES trainer_backend.matches(id),
 player_id uuid NOT NULL REFERENCES trainer_backend.players(id), payload jsonb NOT NULL, payload_hash char(64) NOT NULL,
 state text NOT NULL DEFAULT 'pending' CHECK(state IN ('pending','accepted','rejected')),
 UNIQUE(match_id,player_id,payload_hash)
);
CREATE TABLE trainer_backend.result_decisions (
 id uuid PRIMARY KEY, match_id uuid NOT NULL REFERENCES trainer_backend.matches(id), revision bigint NOT NULL,
 state text NOT NULL, reason text NOT NULL, created_at timestamptz NOT NULL DEFAULT clock_timestamp(), UNIQUE(match_id,revision)
);
CREATE TABLE trainer_backend.result_outcomes (
 match_id uuid NOT NULL REFERENCES trainer_backend.matches(id), subject text NOT NULL, members jsonb NOT NULL,
 score bigint NOT NULL, place integer NOT NULL, outcome text NOT NULL CHECK(outcome IN ('won','draw','lost')), PRIMARY KEY(match_id,subject)
);
CREATE TABLE trainer_backend.board_revisions (
 ruleset_id text PRIMARY KEY REFERENCES trainer_backend.game_rulesets(id), revision bigint NOT NULL DEFAULT 1
);
-- No production game revision is guessed or registered by migration.
