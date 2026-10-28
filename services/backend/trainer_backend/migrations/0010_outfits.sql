-- SPDX-License-Identifier: GPL-3.0-or-later
-- Slot ownership uses the same immutable purchase/ledger flow as profile scenes.
ALTER TABLE trainer_backend.shop_offers
    ALTER COLUMN appearance DROP NOT NULL,
    ADD COLUMN purpose text NOT NULL DEFAULT 'profile_scene'
        CHECK(purpose IN ('profile_scene','outfit_slot')),
    ADD COLUMN slot_number integer,
    ADD CONSTRAINT shop_offer_purpose CHECK (
        (purpose='profile_scene' AND appearance IS NOT NULL AND slot_number IS NULL)
        OR (purpose='outfit_slot' AND appearance IS NULL AND slot_number BETWEEN 1 AND 64 AND slot_number IS NOT NULL)),
    ADD CONSTRAINT outfit_slot_number UNIQUE(slot_number);
CREATE TABLE trainer_backend.outfits (
    player_id uuid NOT NULL,
    offer_id varchar(64) NOT NULL,
    name varchar(32) NOT NULL,
    recipe jsonb NOT NULL CHECK(jsonb_typeof(recipe)='object'),
    revision bigint NOT NULL DEFAULT 1 CHECK(revision>0),
    updated_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    PRIMARY KEY(player_id,offer_id),
    FOREIGN KEY(player_id,offer_id) REFERENCES trainer_backend.owned_items(player_id,offer_id)
);
-- Owner-approved tariff: 100 Sparks once, no level gate. The UI offers the
-- next slot rather than presenting a wall of locked placeholders.
INSERT INTO trainer_backend.shop_offers
    (id,revision,publisher,application,title,description,appearance,price,purpose,slot_number)
SELECT 'outfit.slot.' || n,1,'TrainerOS','traineros','Look slot ' || n,
    'A permanent slot for a complete appearance, including hairstyle.',NULL,100,'outfit_slot',n
FROM generate_series(1,16) AS n;
