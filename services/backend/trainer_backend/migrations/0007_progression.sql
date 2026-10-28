-- SPDX-License-Identifier: GPL-3.0-or-later
CREATE TABLE trainer_backend.progression_accounts (
    player_id uuid PRIMARY KEY REFERENCES trainer_backend.players(id),
    xp bigint NOT NULL DEFAULT 0 CHECK(xp>=0),
    balance bigint NOT NULL DEFAULT 0 CHECK(balance>=0),
    revision bigint NOT NULL DEFAULT 0 CHECK(revision>=0),
    next_check_at timestamptz
);
-- Stable provider identity is bound once. Logout never frees it for reward farming.
CREATE TABLE trainer_backend.reward_links (
    player_id uuid PRIMARY KEY REFERENCES trainer_backend.players(id),
    ra_id varchar(26) NOT NULL UNIQUE,
    username varchar(32) NOT NULL,
    linked_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
CREATE TABLE trainer_backend.reward_link_challenges (
    player_id uuid PRIMARY KEY REFERENCES trainer_backend.players(id),
    ra_id varchar(26) NOT NULL,
    username varchar(32) NOT NULL,
    challenge varchar(64) NOT NULL,
    expires_at timestamptz NOT NULL
);
CREATE TABLE trainer_backend.reward_ledger (
    id uuid PRIMARY KEY,
    player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
    source varchar(24) NOT NULL CHECK(source IN ('ra.achievement','ra.completion')),
    ra_id varchar(26) NOT NULL,
    game_id integer NOT NULL CHECK(game_id>0),
    event_id varchar(32) NOT NULL,
    title varchar(256) NOT NULL,
    xp bigint NOT NULL CHECK(xp>=0),
    sparks bigint NOT NULL CHECK(sparks>=0),
    base_sparks bigint NOT NULL CHECK(base_sparks>=0),
    policy varchar(64) NOT NULL,
    earned_at timestamptz NOT NULL,
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    UNIQUE(ra_id,source,event_id)
);
CREATE INDEX reward_history ON trainer_backend.reward_ledger(player_id,created_at DESC,id);
