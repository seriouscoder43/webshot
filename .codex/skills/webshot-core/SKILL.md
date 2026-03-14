---
name: webshot-core
description: Core repository rules for the webshot repo (structure, naming, safety, tests).
---

# Webshot Core

Use these rules whenever making code changes in this repository.

## Project overview
- Service is a web archive backend similar to archive.today.
- Main service sources, configs, and SQL live under `webshotd/`.
- Shared API and DTO source schemas live under repo-root `schema/`.
- Database bootstrap schemas live only in `webshotd/sql/schema/`.
- Tooling configs at repo root (`.clang-format`, `.editorconfig`, `.pre-commit-config.yaml`) define formatting and checks.

## Project structure
- `webshotd/src/` is the only allowed location for service `.cpp` sources; `webshotd/src/main.cpp` wires core components.
- Shared handlers/components MUST live in `webshotd/include/` (for example, `handler.hpp`, `crud.hpp`), and all such headers MUST use `#pragma once`.
- Service SQL query files MUST live in `webshotd/sql/query/`.
- Service C++ unit tests and pytest tests MUST live under `webshotd/test/`.
- Root `test/` is reserved for repo-level fixtures and helper material; do not add service tests there.
- Chaotic input schemas live in `schema/*.yaml` and are generated into the build tree; edit the source schemas, not generated outputs.

## Style and naming
- Classes MUST use PascalCase (for example, `ByPrefixHandler`).
- Functions and variables MUST use lowerCamelCase.
- Constants MUST use the `kName` form.
- Default parameters in function declarations or definitions are forbidden.
- Namespace rules MUST be strictly followed: reuse the existing `namespace us = userver;` pattern where applicable, and use `::name` for global symbols.
- Use `{}` instead of `std::nullopt` in return statements and obvious initialization sites whenever it compiles.
- Use `size_t`, `int64_t` (not `std::size_t` or `std::int64_t`).
- Never use `Type name = Type(...)`; use `auto name = Type(...)` instead to avoid writing the type twice.
- Filenames MUST be snake_case (for example, `ip_utils.cpp`).
- Declarations and definitions MUST exactly match (names and signatures).
- Do not introduce duplicate code; factor common logic into reusable helpers.
- Code MUST be designed to handle adversarial input too.
- Prefer `std::begin`/`std::end` over calling `.begin()`/`.end()` on containers when passing iterators.
- Postfix arithmetic (`++`, `--`) MUST be used by default.
- Never set default values in code for component config options; require them in static config or config_vars.
- Class members must not use a trailing underscore naming style; use regular lowerCamelCase for member variables.
- Never call `std::chrono::system_clock::now()`; use `userver::utils::datetime::Now()` instead.
- Mutable lambdas are forbidden; capture-by-mutable is not allowed in this codebase.
- Never use `return ReturnType(...)`; when constructing a value to return, prefer `return {...};` wherever it compiles.

## Testing expectations
- C++ service tests use `userver::utest` and are wired from `webshotd/test/CMakeLists.txt`.
- Pytest-based service and testsuite coverage lives under `webshotd/test/` and uses `pytest_userver` plus yandex-taxi-testsuite.
- When a change affects schemas, SQL, config, or request handling, update or add service tests in `webshotd/test/`.

## [[nodiscard]] usage
- Do not annotate destructors, move operations, or obvious mutators.
- Favor annotating; compilers will surface accidental value drops.
- The `[[nodiscard]]` rules in this section are mandatory and MUST be followed strictly.
