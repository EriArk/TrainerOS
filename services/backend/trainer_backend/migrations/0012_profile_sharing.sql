-- SPDX-License-Identifier: GPL-3.0-or-later
CREATE TABLE trainer_backend.profile_sharing (
    player_id uuid PRIMARY KEY REFERENCES trainer_backend.players(id),
    revision bigint NOT NULL DEFAULT 0,
    level boolean NOT NULL DEFAULT true,
    shelf boolean NOT NULL DEFAULT true,
    history boolean NOT NULL DEFAULT false,
    activity boolean NOT NULL DEFAULT false,
    favorites boolean NOT NULL DEFAULT false,
    favorite_games jsonb NOT NULL DEFAULT '[]'::jsonb CHECK(jsonb_typeof(favorite_games)='array' AND jsonb_array_length(favorite_games)<=5)
);
