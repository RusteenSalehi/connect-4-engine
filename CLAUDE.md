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
