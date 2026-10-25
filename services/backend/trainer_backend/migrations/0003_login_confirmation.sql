-- SPDX-License-Identifier: GPL-3.0-or-later
ALTER TABLE trainer_backend.login_attempts DROP CONSTRAINT login_attempts_status_check;
ALTER TABLE trainer_backend.login_attempts ADD CONSTRAINT login_attempts_status_check
    CHECK(status IN ('created','confirming','authorizing','verifying','verified','claimed','failed'));
ALTER TABLE trainer_backend.login_attempts ADD COLUMN confirm_failures integer NOT NULL DEFAULT 0
    CHECK(confirm_failures BETWEEN 0 AND 5);
