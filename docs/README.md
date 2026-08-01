# Selene documentation

Selene v2: modernization of **ELENA 1.9.23 / 2.0** (2015, Alex Rakov).
Round 1 — the ELENA 1.5.0.0 experiment — lives complete in
`experimental_version/`, including its own ~17k lines of docs
(`experimental_version/docs/`), which remain the authoritative argument for
the design decisions Selene carries forward.

| Doc | Contents |
|---|---|
| [01-codebase-map.md](01-codebase-map.md) | The 1.9.23/2.0 system as it is: tree, build pipeline, e-code machine, module format, compiler pipeline, JIT/VM anatomy, FFI mechanisms, tool verdicts, 64-bit/endian hazards |
| [02-modernization-plan.md](02-modernization-plan.md) | Goals, decisions inherited from round 1, what is new in 2.0, module format v2, phases P0–P6, deletion order, risks |

Reference material inherited from round 1 (read from
`experimental_version/docs/`): `plan/17` (LLVM backend), `plan/18` (FFI),
`plan/19` (runtime in C), `plan/20` (OS development constraints),
`plan/23` (failure ABI), `architecture/22` (platform layer rules).

`doc/` (singular, repo root) is the original 2009–2015 author
documentation; useful (`doc/tech/bytecode.txt`, `doc/tech/knowhow.txt`) but
partly stale — `docs/` supersedes it where they disagree.
