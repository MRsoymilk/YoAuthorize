CREATE EXTENSION IF NOT EXISTS pgcrypto;

CREATE TYPE user_role AS ENUM ('user', 'admin');
CREATE TYPE user_status AS ENUM ('pending', 'active', 'disabled');
CREATE TYPE license_status AS ENUM ('active', 'revoked');

CREATE TABLE users (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    email text NOT NULL,
    name text NOT NULL,
    password_hash text NOT NULL,
    role user_role NOT NULL DEFAULT 'user',
    status user_status NOT NULL DEFAULT 'pending',
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now(),
    CONSTRAINT users_email_normalized CHECK (email = lower(email))
);
CREATE UNIQUE INDEX users_email_unique ON users (lower(email));

CREATE TABLE email_tokens (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    purpose text NOT NULL CHECK (purpose IN ('verify_email', 'reset_password')),
    token_hash bytea NOT NULL UNIQUE,
    expires_at timestamptz NOT NULL,
    used_at timestamptz,
    created_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX email_tokens_user_purpose ON email_tokens(user_id, purpose);

CREATE TABLE web_sessions (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id uuid NOT NULL REFERENCES users(id) ON DELETE CASCADE,
    token_hash bytea NOT NULL UNIQUE,
    csrf_hash bytea NOT NULL,
    expires_at timestamptz NOT NULL,
    last_seen_at timestamptz NOT NULL DEFAULT now(),
    created_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX web_sessions_user ON web_sessions(user_id);

CREATE TABLE products (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    code text NOT NULL UNIQUE CHECK (code ~ '^[A-Za-z0-9_-]{1,100}$'),
    name text NOT NULL,
    description text,
    created_at timestamptz NOT NULL DEFAULT now()
);
CREATE TABLE features (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    product_id uuid NOT NULL REFERENCES products(id) ON DELETE CASCADE,
    code text NOT NULL CHECK (code ~ '^[A-Za-z0-9_.-]{1,100}$'),
    name text NOT NULL,
    created_at timestamptz NOT NULL DEFAULT now(),
    UNIQUE(product_id, code)
);

CREATE TABLE licenses (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    license_key text NOT NULL UNIQUE,
    product_id uuid NOT NULL REFERENCES products(id),
    user_id uuid REFERENCES users(id),
    status license_status NOT NULL DEFAULT 'active',
    license_type text NOT NULL CHECK (license_type IN ('trial', 'subscription', 'permanent')),
    not_before timestamptz NOT NULL DEFAULT now(),
    expires_at timestamptz,
    device_limit integer NOT NULL CHECK (device_limit > 0 AND device_limit <= 10000),
    max_sessions integer NOT NULL DEFAULT 1 CHECK (max_sessions > 0 AND max_sessions <= 10000),
    created_at timestamptz NOT NULL DEFAULT now(),
    updated_at timestamptz NOT NULL DEFAULT now(),
    CHECK ((license_type = 'permanent' AND expires_at IS NULL) OR (license_type <> 'permanent' AND expires_at IS NOT NULL AND expires_at > not_before)),
    UNIQUE(id, product_id)
);
CREATE TABLE license_features (
    license_id uuid NOT NULL REFERENCES licenses(id) ON DELETE CASCADE,
    feature_id uuid NOT NULL REFERENCES features(id) ON DELETE RESTRICT,
    PRIMARY KEY(license_id, feature_id)
);

CREATE TABLE activation_codes (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    product_id uuid NOT NULL REFERENCES products(id),
    license_id uuid,
    code_hash bytea NOT NULL UNIQUE,
    code_hint text NOT NULL,
    activation_limit integer NOT NULL DEFAULT 1 CHECK (activation_limit > 0 AND activation_limit <= 10000),
    activation_count integer NOT NULL DEFAULT 0 CHECK (activation_count >= 0),
    expires_at timestamptz,
    disabled_at timestamptz,
    created_at timestamptz NOT NULL DEFAULT now(),
    CHECK (activation_count <= activation_limit),
    FOREIGN KEY (license_id, product_id) REFERENCES licenses(id, product_id)
);

CREATE TABLE devices (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    user_id uuid REFERENCES users(id),
    machine_hash bytea NOT NULL,
    machine_hint text NOT NULL,
    name text NOT NULL DEFAULT 'Device',
    platform text,
    created_at timestamptz NOT NULL DEFAULT now(),
    last_seen_at timestamptz NOT NULL DEFAULT now(),
    UNIQUE(machine_hash)
);
CREATE TABLE activations (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    license_id uuid NOT NULL REFERENCES licenses(id),
    device_id uuid NOT NULL REFERENCES devices(id),
    activation_code_id uuid NOT NULL REFERENCES activation_codes(id),
    idempotency_key text,
    license_blob bytea NOT NULL,
    activated_at timestamptz NOT NULL DEFAULT now(),
    unbound_at timestamptz
);
CREATE UNIQUE INDEX activations_active_license_device_unique ON activations(license_id, device_id) WHERE unbound_at IS NULL;
CREATE UNIQUE INDEX activations_idempotency_unique ON activations(activation_code_id, idempotency_key) WHERE idempotency_key IS NOT NULL;

CREATE TABLE device_tokens (
    id uuid PRIMARY KEY DEFAULT gen_random_uuid(),
    activation_id uuid NOT NULL REFERENCES activations(id) ON DELETE CASCADE,
    token_hash bytea NOT NULL UNIQUE,
    expires_at timestamptz NOT NULL,
    revoked_at timestamptz,
    created_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX device_tokens_activation ON device_tokens(activation_id);

CREATE TABLE signing_keys (
    key_id text PRIMARY KEY,
    algorithm text NOT NULL DEFAULT 'Ed25519' CHECK (algorithm = 'Ed25519'),
    public_key bytea NOT NULL CHECK (octet_length(public_key) = 32),
    status text NOT NULL DEFAULT 'active' CHECK (status IN ('active', 'disabled', 'revoked')),
    created_at timestamptz NOT NULL DEFAULT now(),
    not_after timestamptz
);

CREATE TABLE audit_logs (
    id bigserial PRIMARY KEY,
    actor_user_id uuid REFERENCES users(id),
    action text NOT NULL,
    target_type text,
    target_id text,
    ip_address inet,
    metadata jsonb NOT NULL DEFAULT '{}'::jsonb,
    created_at timestamptz NOT NULL DEFAULT now()
);
CREATE INDEX audit_logs_created ON audit_logs(created_at DESC);

CREATE TABLE outbox_events (
    id bigserial PRIMARY KEY,
    topic text NOT NULL,
    aggregate_id text NOT NULL,
    payload jsonb NOT NULL,
    created_at timestamptz NOT NULL DEFAULT now(),
    published_at timestamptz,
    attempts integer NOT NULL DEFAULT 0
);
CREATE INDEX outbox_unpublished ON outbox_events(id) WHERE published_at IS NULL;

-- Security history is append-only at the database layer.
CREATE FUNCTION reject_audit_mutation() RETURNS trigger LANGUAGE plpgsql AS $$
BEGIN RAISE EXCEPTION 'audit logs are append-only'; END $$;
CREATE TRIGGER audit_logs_append_only BEFORE UPDATE OR DELETE ON audit_logs
FOR EACH ROW EXECUTE FUNCTION reject_audit_mutation();
