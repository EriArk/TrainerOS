ALTER TABLE trainer_backend.login_attempts
    ADD COLUMN return_uri text,
    ADD COLUMN handoff_hash char(64),
    ADD COLUMN expected_subject varchar(32);
