# Atlas

Atlas is a standalone [UCI](https://en.wikipedia.org/wiki/Universal_Chess_Interface)
chess engine in C++20 — bitboards, magic-bitboard sliding move generation,
make/unmake with incremental Zobrist hashing, and a negamax search with the
usual modern pruning stack (TT, null-move pruning, LMR, PVS, aspiration
windows, quiescence with SEE, killer/history move ordering).

This project started as a UCI wrapper around
[chess.html](https://github.com/rithikbanerjee314/chess)'s browser-based
JS engine, ported over for standalone strength testing. It's since been
rewritten from scratch in C++ and decoupled entirely — no ongoing sync with
that project, free to diverge and improve on its own.

## Current strength

**~2468 ± 85 Elo (95% CI)**, anchored against Stockfish at
2200/2500/2800/3100 `UCI_Elo`, 120 games at 30+0.3, computed with
`gauntlet/compute-elo.js`. Two independent measurements confirm the C++
rewrite over the prior JS version (which was itself ~2130 ± 88):

- **Anchored strength**: 2130 → 2468, a ~340 Elo jump.
- **Head-to-head self-play** (120 games, JS engine preserved for this one
  comparison): C++ engine scored **116-3-1 (97.1%)**, Elo difference
  +609 ± 486, LOS 100%.

## Why C++

A hobby engine can reach 2500+ Elo without a neural (NNUE) evaluation —
[`tuna`](https://github.com/billchow98/chess), a comparable bitboard C++
engine with a classical eval, hits 2505.9 in SPRT-validated play. What it
takes is raw search depth, and depth compounds directly with speed:
doubling nodes-per-second is worth roughly one more ply, worth roughly
50-70 Elo. The JS version's architecture had several compounding
inefficiencies for that specific goal — a naive array board copied in full
on every search node, no bitboards, a position hash recomputed from scratch
every node instead of incrementally maintained. This rewrite fixes all of
that at once: `Position::makeMove`/`unmakeMove` mutate in place (no copies),
magic bitboards answer "what does this rook attack" in O(1), and the
Zobrist hash is XOR'd incrementally.

## Build

Requires a C++20 compiler and CMake. No other dependencies.

```
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Produces `build/bin/atlas` (the UCI engine) and `build/bin/perft` (the
move-generation correctness harness).

On Windows with MSVC, run both commands from a Developer Command Prompt (or
`vcvarsall.bat x64` first) so `cl`/`cmake` are on `PATH`.

## Manual test

```
./build/bin/atlas
uci
isready
position startpos
go movetime 1000
quit
```

You should see `id name ...` / `uciok`, `readyok`, then an `info ...` /
`bestmove ...` line about a second after `go`.

## Correctness: perft

`build/bin/perft` runs known-node-count checks (startpos, Kiwipete, CPW
position 4) — the standard, rigorous way to validate bitboard move
generation, since move-gen bugs are the most common and most silently
corrupting bug class in a chess engine. All pass exactly, including a
depth-6 startpos check (119,060,324 nodes).

## UCI protocol smoke test

```
node test/uci_smoke.js [path-to-exe]
```

Spawns the engine as a real child process and drives it through a UCI
handshake plus two `go` searches (fixed-depth and `movetime`), keeping
stdin open between commands — closing it immediately after `go` triggers
the engine's own EOF-cleanup `stop`, which would cut a real search short
and isn't representative of how a GUI actually behaves.

## Rating your engine locally with cutechess-cli

The most direct way to get a real Elo estimate: play a gauntlet against
opponents of known strength and compute the result with a proper rating
tool.

1. Install [cutechess-cli](https://github.com/cutechess/cutechess) and
   [Stockfish](https://stockfishchess.org/). On Windows, both are on winget:
   `winget install Stockfish.Stockfish` and `winget install CuteChess.CuteChess`
   (the latter bundles `cutechess-cli.exe` alongside the GUI).
2. Run `gauntlet/run-gauntlet.sh`. It pits the compiled engine against
   Stockfish at several `UCI_LimitStrength` levels using a 16-line opening
   book (`gauntlet/openings.pgn`) so games aren't repetitive, and writes a
   timestamped PGN to `gauntlet/results/`:

   ```
   STOCKFISH_CMD=/path/to/stockfish CUTECHESS_CMD=/path/to/cutechess-cli \
     ENGINE_CMD=./build/bin/atlas ./gauntlet/run-gauntlet.sh
   ```

   Tune it via env vars: `TC` (time control), `ROUNDS` (per opponent, each
   round is 2 games with colors swapped), `CONCURRENCY`, `ELOS`
   (space-separated Stockfish levels — raise these as the engine gets
   stronger so the anchors still bracket its real strength).
3. Get an anchored Elo estimate from the PGN. If you have
   [`ordo`](https://github.com/michiguel/Ordo) or `bayeselo` built/installed,
   use those. Otherwise, `gauntlet/compute-elo.js` does the same thing with
   zero dependencies — a maximum-likelihood logistic fit of a single unknown
   rating (your engine) against the fixed, known ratings of the `SF-<elo>`
   opponents, with a 95% confidence interval:

   ```
   node gauntlet/compute-elo.js gauntlet/results/<file>.pgn Atlas \
     SF-2200=2200 SF-2500=2500 SF-2800=2800 SF-3100=3100
   ```

   Add `--exclude-time-forfeits` to drop games ended by `[Termination "time
   forfeit"]` from the fit — a flag on move 6 out of an opening book is a
   giveaway that a game was lost to test-machine scheduling contention
   rather than chess, and including it as a normal loss would bias the
   estimate. Costs nothing to check even when the effect turns out small.

## Submitting to CCRL / CEGT

These are the widely-cited "official" rating lists. They run your engine
themselves on standardized hardware against their existing pool, so no local
setup is needed beyond having a working UCI binary — but there are
submission queues and eligibility rules (deterministic play, no online
learning, open source, etc.), and it can take a while to get a result.

- [CCRL](https://computerchess.org.uk/ccrl/)
- [CEGT](http://www.cegt.net/)

## Architecture

| File | Responsibility |
|---|---|
| `src/bitboard.hpp/.cpp` | `uint64_t` board type, popcount/bitscan helpers |
| `src/magic.hpp/.cpp` | Knight/king/pawn attack tables; magic-bitboard rook/bishop/queen sliding attacks (magic numbers found via randomized search at startup) |
| `src/position.hpp/.cpp` | `Position` (12 piece bitboards + mailbox mirror), `makeMove`/`unmakeMove` with an undo stack, incremental Zobrist hashing, FEN parsing |
| `src/movegen.hpp/.cpp` | Pseudo-legal and fully-legal move generation (legality via make/unmake + check test, no board copies) |
| `src/eval.hpp/.cpp` | Tapered classical evaluation: material, PST, pawn structure, mobility, rook/knight bonuses, threats, space, king safety |
| `src/tt.hpp` | Zobrist-keyed transposition table, always-replace-by-depth |
| `src/search.hpp/.cpp` | Negamax/PVS with the full pruning stack, quiescence with SEE, repetition detection |
| `src/uci.hpp/.cpp` | UCI protocol loop; search runs on a background `std::thread` with an atomic stop flag, so `stop` genuinely preempts an in-progress search (the JS version's `stop` was accepted but not preemptive — this is a real improvement, not just a port) |
| `test/perft.cpp` | Move-generation correctness harness |
| `test/uci_smoke.js` | End-to-end UCI protocol test |
| `gauntlet/` | `cutechess-cli`-based self-play/Stockfish-gauntlet tooling and the dependency-free Elo estimator (unchanged from the JS version — UCI is a text protocol, so this tooling doesn't care what language the engine is written in) |

## Known limitations / next steps

- **No NNUE.** Classical hand-crafted evaluation is enough to reach 2500+
  (see the `tuna` reference point above); NNUE would be the next lever for
  pushing further, but requires training data and a trainer this project
  doesn't have yet.
- **No PEXT/BMI2 sliding attacks.** Plain magic bitboards are used instead —
  portable to any x86-64 CPU, with a marginally larger constant factor than
  PEXT. A valid future micro-optimization, not required to hit 2500 per the
  `tuna` reference point.
- **Single-threaded search.** Lazy SMP (multi-threaded search sharing a TT)
  is a standard further Elo lever not yet implemented.
- **No configurable UCI options.** `setoption` is accepted but ignored —
  hash size is fixed at 64MB; no `Threads`/`UCI_Elo` limiting, etc.
