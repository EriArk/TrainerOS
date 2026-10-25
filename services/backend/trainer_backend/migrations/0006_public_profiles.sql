-- SPDX-License-Identifier: GPL-3.0-or-later
CREATE TABLE trainer_backend.public_profiles (
    player_id uuid PRIMARY KEY REFERENCES trainer_backend.players(id),
    display_name varchar(64) NOT NULL,
    avatar jsonb CHECK(avatar IS NULL OR (jsonb_typeof(avatar)='object' AND octet_length(avatar::text)<=2048)),
    revision bigint NOT NULL DEFAULT 1 CHECK(revision>0),
    updated_at timestamptz NOT NULL DEFAULT clock_timestamp()
);
