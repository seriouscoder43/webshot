# Postgres Schema Management

`webshotd` uses `pgmigrate` migrations as the source of truth for Postgres schema.

## Layout

- `capture_meta_db/`
  - `migrations.yml`: pgmigrate configuration (schema, serial version checks, etc.)
  - `migrations/V####__*.sql`: ordered migrations
- `shared_state_db/`
  - `migrations.yml`
  - `migrations/V####__*.sql`

## Conventions

- Versions are integers (`V0001`, `V0002`, ...) and must be strictly increasing with no gaps.
- Migrations should be transactional by default.
- Existing local DBs created before migrations were introduced require an explicit baseline; the runtime will fail with an exact `pgmigrate baseline` command when it detects a legacy schema.

