-- SPDX-License-Identifier: GPL-3.0-or-later
-- Prize facts never depend on a client animation or a model being installed.
CREATE TABLE trainer_backend.prize_grants (
    id uuid PRIMARY KEY,
    sequence bigint GENERATED ALWAYS AS IDENTITY UNIQUE,
    player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
    event_key varchar(128) NOT NULL,
    kind text NOT NULL CHECK(kind IN ('clothing','outfit_slot','beaten','completed','hardcore')),
    item_id varchar(64),
    level integer CHECK(level>=2),
    game_id integer CHECK(game_id>0),
    title varchar(256) NOT NULL,
    earned_at timestamptz NOT NULL,
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    policy text NOT NULL DEFAULT 'prizes-2026-10-v1',
    UNIQUE(player_id,event_key), UNIQUE(player_id,id)
);
CREATE TABLE trainer_backend.prize_catalogue (
    id varchar(64) PRIMARY KEY,
    game_id integer UNIQUE CHECK(game_id>0),
    level integer UNIQUE CHECK(level>=2),
    title varchar(256) NOT NULL,
    CHECK((game_id IS NULL) <> (level IS NULL))
);
-- No prize models/assignments yet. Catalogue rows will be owner-curated.
CREATE TABLE trainer_backend.outfit_slots (
    player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
    slot_id varchar(64) NOT NULL,
    purchase_id uuid REFERENCES trainer_backend.purchases(id),
    grant_id uuid,
    PRIMARY KEY(player_id,slot_id),
    FOREIGN KEY(player_id,grant_id) REFERENCES trainer_backend.prize_grants(player_id,id),
    CHECK((purchase_id IS NULL) <> (grant_id IS NULL))
);
INSERT INTO trainer_backend.outfit_slots(player_id,slot_id,purchase_id)
SELECT i.player_id,i.offer_id,i.purchase_id FROM trainer_backend.owned_items i
JOIN trainer_backend.shop_offers s ON s.id=i.offer_id WHERE s.purpose='outfit_slot';
ALTER TABLE trainer_backend.outfits DROP CONSTRAINT outfits_player_id_offer_id_fkey;
ALTER TABLE trainer_backend.outfits ADD FOREIGN KEY(player_id,offer_id)
    REFERENCES trainer_backend.outfit_slots(player_id,slot_id);
CREATE TABLE trainer_backend.prize_receipts (
    player_id uuid NOT NULL,
    grant_id uuid NOT NULL,
    presented_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(player_id,grant_id),
    FOREIGN KEY(player_id,grant_id) REFERENCES trainer_backend.prize_grants(player_id,id)
);
CREATE TABLE trainer_backend.prize_shelf (
    player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
    position integer NOT NULL CHECK(position BETWEEN 0 AND 4),
    game_id integer NOT NULL CHECK(game_id>0),
    PRIMARY KEY(player_id,position), UNIQUE(player_id,game_id)
);
CREATE INDEX prize_player_sequence ON trainer_backend.prize_grants(player_id,sequence);
