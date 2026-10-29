-- SPDX-License-Identifier: GPL-3.0-or-later
-- Only maintenance credentials can insert a grant. Runtime can read receipts.
CREATE TABLE trainer_backend.operator_spark_grants (
    id uuid PRIMARY KEY REFERENCES trainer_backend.operation_receipts(id),
    player_id uuid NOT NULL REFERENCES trainer_backend.players(id),
    sparks bigint NOT NULL CHECK(sparks BETWEEN 1 AND 1000000),
    reason varchar(200) NOT NULL CHECK(length(trim(reason))>0),
    created_at timestamptz NOT NULL DEFAULT clock_timestamp(),
    UNIQUE(id,player_id,sparks)
);
ALTER TABLE trainer_backend.reward_ledger
    ADD COLUMN operator_grant_id uuid UNIQUE,
    ADD CONSTRAINT operator_grant_owner_amount FOREIGN KEY(operator_grant_id,player_id,sparks)
        REFERENCES trainer_backend.operator_spark_grants(id,player_id,sparks),
    DROP CONSTRAINT ledger_source_amounts,
    ADD CONSTRAINT ledger_source_amounts CHECK (
        (operator_grant_id IS NULL AND (
            (source IN ('ra.achievement','ra.completion') AND sparks>=0 AND ra_id IS NOT NULL AND game_id IS NOT NULL AND purchase_id IS NULL
                AND base_reward_id IS NULL AND support_multiplier IS NULL AND support_receipt_id IS NULL)
            OR (source='shop.purchase' AND sparks<0 AND xp=0 AND base_sparks=0 AND ra_id IS NULL AND game_id IS NULL AND purchase_id IS NOT NULL
                AND base_reward_id IS NULL AND support_multiplier IS NULL AND support_receipt_id IS NULL)
            OR (source='support.topup' AND sparks>0 AND xp=0 AND base_sparks=0 AND ra_id IS NULL AND game_id IS NULL AND purchase_id IS NULL
                AND base_reward_id IS NOT NULL AND support_multiplier IS NOT NULL AND support_multiplier IN (2,3) AND support_receipt_id IS NOT NULL)
        ))
        OR (source='operator.test_grant' AND sparks>0 AND xp=0 AND base_sparks=0 AND ra_id IS NULL AND game_id IS NULL
            AND purchase_id IS NULL AND base_reward_id IS NULL AND support_multiplier IS NULL AND support_receipt_id IS NULL
            AND operator_grant_id IS NOT NULL)
    );
