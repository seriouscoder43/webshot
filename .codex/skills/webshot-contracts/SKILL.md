---
name: webshot-contracts
description: API/schema and change hygiene for the webshot repo (schemas, generated outputs, commits, PRs).
---

# Webshot Contracts

Use this when changing the HTTP API, schema, or preparing commits/PRs.

## API and schema alignment
- Public HTTP API is defined in `schema/webshot.yaml`.
- Additional DTO source schemas live in `schema/cdp.yaml`, `schema/browsertrix_pages.yaml`, and `schema/wacz.yaml`.
- `webshotd/CMakeLists.txt` feeds those schemas into `userver_target_generate_chaotic`, which generates DTOs into the build tree. Edit source schemas and rebuild; do not hand-edit generated outputs.
- Keep schemas, handlers, SQL call sites, and service tests aligned in the same change.

## SQL contracts
- Database bootstrap schemas live in `webshotd/sql/schema/capture_meta_db.sql` and `webshotd/sql/schema/shared_state_db.sql`.
- Query contracts live in `webshotd/sql/query/*.sql`; when a query shape changes, update the matching C++ call site and tests together.

## Commits and pull requests
- Follow `conventional_commits.md`; commit subjects use Conventional Commit style (such as `feat:` or `fix:`).
- Pull requests summarize behavior changes, list affected endpoints, schema or SQL contract changes, and any config or runtime impact.
