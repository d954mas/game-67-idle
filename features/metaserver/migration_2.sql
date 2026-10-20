-- Version 2: the console signs in with a password alone. Dropping the columns
-- also drops the console's column grant on last_totp_step.
ALTER TABLE ops.admin_users DROP COLUMN totp_secret, DROP COLUMN last_totp_step;
