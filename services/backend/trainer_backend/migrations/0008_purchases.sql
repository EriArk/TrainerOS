-- SPDX-License-Identifier: GPL-3.0-or-later
CREATE TABLE trainer_backend.shop_offers (
    id varchar(64) PRIMARY KEY,
    revision integer NOT NULL CHECK(revision>0),
    publisher varchar(64) NOT NULL,
    application varchar(64) NOT NULL,
    title varchar(128) NOT NULL,
    description varchar(512) NOT NULL,
    appearance varchar(32) NOT NULL UNIQUE,
    price bigint NOT NULL CHECK(price>0),
    minimum_level integer NOT NULL DEFAULT 1 CHECK(minimum_level>0),
    available boolean NOT NULL DEFAULT true,
    kind text NOT NULL DEFAULT 'permanent_license' CHECK(kind='permanent_license')
);
INSERT INTO trainer_backend.shop_offers(id,revision,publisher,application,title,description,appearance,price) VALUES
('profile.aurora',1,'TrainerOS','traineros','Aurora','A lantern-lit forest clearing beneath the northern lights. Your own quiet checkpoint.','aurora',250),
('profile.dusk',1,'TrainerOS','traineros','Dusk','An arcade rooftop above a sunset coast. One more game before the stars come out.','dusk',250),
('profile.orbit',1,'TrainerOS','traineros','Orbit','An observation deck above a ringed planet. A home base between adventures.','orbit',250);
-- Row locking needs UPDATE privilege in PostgreSQL. Keep the runtime catalogue
-- read-only and expose only this bounded lock/read function instead.
CREATE FUNCTION trainer_backend.lock_shop_offer(offer text) RETURNS SETOF trainer_backend.shop_offers
LANGUAGE sql SECURITY DEFINER SET search_path=pg_catalog,trainer_backend AS $$
    SELECT * FROM trainer_backend.shop_offers WHERE id=offer FOR SHARE;
$$;
REVOKE ALL ON FUNCTION trainer_backend.lock_shop_offer(text) FROM PUBLIC;
CREATE TABLE trainer_backend.purchases (
    id uuid PRIMARY KEY,
    player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
    operation_id uuid NOT NULL,
    offer_id varchar(64) NOT NULL REFERENCES trainer_backend.shop_offers(id),
    offer_revision integer NOT NULL,
    price bigint NOT NULL CHECK(price>0),
    title varchar(128) NOT NULL,
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    UNIQUE(player_id,operation_id),
    UNIQUE(player_id,offer_id)
);
CREATE TABLE trainer_backend.owned_items (
    player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
    offer_id varchar(64) NOT NULL REFERENCES trainer_backend.shop_offers(id),
    purchase_id uuid NOT NULL UNIQUE REFERENCES trainer_backend.purchases(id),
    PRIMARY KEY(player_id,offer_id)
);
CREATE TABLE trainer_backend.profile_appearances (
    player_id uuid PRIMARY KEY REFERENCES trainer_backend.players(id),
    offer_id varchar(64),
    FOREIGN KEY(player_id,offer_id) REFERENCES trainer_backend.owned_items(player_id,offer_id)
);
ALTER TABLE trainer_backend.reward_ledger
    DROP CONSTRAINT reward_ledger_source_check,
    DROP CONSTRAINT reward_ledger_sparks_check,
    ALTER COLUMN ra_id DROP NOT NULL,
    ALTER COLUMN game_id DROP NOT NULL,
    ALTER COLUMN event_id TYPE varchar(64),
    ADD COLUMN purchase_id uuid UNIQUE REFERENCES trainer_backend.purchases(id),
    ADD CONSTRAINT ledger_source_amounts CHECK (
        (source IN ('ra.achievement','ra.completion') AND sparks>=0 AND ra_id IS NOT NULL AND game_id IS NOT NULL AND purchase_id IS NULL)
        OR (source='shop.purchase' AND sparks<0 AND xp=0 AND base_sparks=0 AND ra_id IS NULL AND game_id IS NULL AND purchase_id IS NOT NULL)
    );
