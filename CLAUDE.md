# Engineering rules for this repo

- Build the simplest thing that meets the spec: fewest concepts, files, and lines that stay readable. Readability beats both cleverness and brevity.
- No speculative generality: no options, abstraction layers, templates, config, or helper functions that nothing uses yet.
- Prefer plain structs and free functions. No class hierarchies. No templates unless unavoidable.
- One way to do each thing.
- Do not optimize without a benchmark showing it matters. The bitboard representation is the one deliberate exception.
- Comments explain WHY, briefly.
- Before adding anything not in the spec, ask me.
- If simplicity conflicts with the spec, flag it and ask; do not silently choose.
- Non-negotiable (my resume claims these, so they must exist): negamax with alpha-beta pruning, iterative deepening, a transposition table keyed by Zobrist hashing, an evaluation function with all weights in one place, benchmarks reporting nodes, nodes per second, and time per move by depth, and tests including the alpha-beta vs minimax equivalence test.
- No em dashes anywhere.
- Claude Code runs inside WSL Ubuntu. All builds, tests, benchmarks, git, and Node commands run natively in Linux. Never report tests as passing unless the actual test output with counts was printed.

## End-of-phase routine (run at the end of every prompt)
1. Release and Debug builds with zero warnings; all tests pass.
2. One commit per logical step, descriptive messages, never push.
3. Write reports/<phase-name>.md using the same 8-section format as the checkpoint reports.
4. Do not write to the Obsidian vault. Prepend a dated entry to reports/devlog.md (what was built, decisions, problems and fixes, test status, key numbers, open questions); update reports/overview.md (status, next phase); update reports/architecture.md (components and how they interact).
5. Print a short terminal summary, then stop.
