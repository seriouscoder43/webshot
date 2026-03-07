# Repository Guidelines

Detailed repository rules live in local Codex skills under `.codex/skills/`.

Reminder:
- Use `webshot-core` for any code changes.
- Use `webshot-workflows` for build/run/test tasks.
- Use `webshot-contracts` for API/schema/DTO changes and commits/PRs.
- Use `webshot-docs` for docs lookup and upstream research.

# General rules
- Never implement backwards compatibility or silent fallbacks unless told to do so.
- Prefer failing hard over silent fallbacks.
- Never introduce environment variables unless told to do so.

# Response discipline
- Do not respond with large blocks of code; show only short, focused snippets when necessary, or omit code entirely and describe changes instead.
- Verify, don't recall: reread active code/logs; test exact endpoints.
- Prefer authoritative data: no guessing or fallback to stale/synthetic for critical logic.
- Always say what the contents of replies are based on (memory, repo code, docs, tool output).
