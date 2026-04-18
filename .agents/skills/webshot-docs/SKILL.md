---
name: webshot-docs-home
description: How to look up docs for the webshot repo and its dependencies
---

# Webshot Docs Lookup

Use this when searching docs, APIs, or upstream behavior.

## Docs sources priority
- Start with repo-pinned sources for version/context: `devenv.yaml`, `devenv.lock`, `devenv/toolchain.nix`, `devenv/*.nix`, and `webshotd/CMakeLists.txt`.
- Repo code and schemas are authoritative for local behavior: use `webshotd/config/`, `webshotd/sql/`, `webshotd/test/`, and `schema/*.yaml` before assuming upstream defaults.
