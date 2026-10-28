-- SPDX-License-Identifier: GPL-3.0-or-later
CREATE TABLE trainer_backend.support_receipts (
    id uuid PRIMARY KEY,
    player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
    provider varchar(32) NOT NULL,
    subject varchar(128) NOT NULL,
    event_id varchar(128) NOT NULL,
    revision bigint NOT NULL CHECK(revision>0),
    multiplier integer NOT NULL CHECK(multiplier IN (1,2,3)),
    observed_at timestamptz NOT NULL,
    valid_until timestamptz NOT NULL,
    evidence_hash char(64) NOT NULL,
    payload_hash char(64) NOT NULL,
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    UNIQUE(provider,event_id), UNIQUE(id,player_id)
);
CREATE TABLE trainer_backend.support_memberships (
    provider varchar(32) NOT NULL,
    subject varchar(128) NOT NULL,
    player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
    receipt_id uuid NOT NULL,
    PRIMARY KEY(provider,subject), UNIQUE(provider,player_id),
    FOREIGN KEY(receipt_id,player_id) REFERENCES trainer_backend.support_receipts(id,player_id)
);
ALTER TABLE trainer_backend.reward_ledger
    ADD CONSTRAINT reward_owner_identity UNIQUE(id,player_id),
    ADD COLUMN base_reward_id uuid,
    ADD COLUMN support_multiplier integer,
    ADD COLUMN support_receipt_id uuid,
    ADD CONSTRAINT support_reward_owner FOREIGN KEY(base_reward_id,player_id) REFERENCES trainer_backend.reward_ledger(id,player_id),
    ADD CONSTRAINT support_receipt_owner FOREIGN KEY(support_receipt_id,player_id) REFERENCES trainer_backend.support_receipts(id,player_id),
    ADD CONSTRAINT support_reward_step UNIQUE(base_reward_id,support_multiplier),
    DROP CONSTRAINT ledger_source_amounts,
    ADD CONSTRAINT ledger_source_amounts CHECK (
        (source IN ('ra.achievement','ra.completion') AND sparks>=0 AND ra_id IS NOT NULL AND game_id IS NOT NULL AND purchase_id IS NULL
            AND base_reward_id IS NULL AND support_multiplier IS NULL AND support_receipt_id IS NULL)
        OR (source='shop.purchase' AND sparks<0 AND xp=0 AND base_sparks=0 AND ra_id IS NULL AND game_id IS NULL AND purchase_id IS NOT NULL
            AND base_reward_id IS NULL AND support_multiplier IS NULL AND support_receipt_id IS NULL)
        OR (source='support.topup' AND sparks>0 AND xp=0 AND base_sparks=0 AND ra_id IS NULL AND game_id IS NULL AND purchase_id IS NULL
            AND base_reward_id IS NOT NULL AND support_multiplier IS NOT NULL AND support_multiplier IN (2,3) AND support_receipt_id IS NOT NULL)
    );
