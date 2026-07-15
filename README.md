# chess-bot-uci

A [UCI](https://en.wikipedia.org/wiki/Universal_Chess_Interface)-speaking
command-line engine, so the search/eval from
[chess.html](https://github.com/rithikbanerjee314/chess) can be tested with
standard chess-engine tooling (`cutechess-cli`, Arena, CCRL/CEGT submission,
a Lichess bot bridge, etc.) instead of only being playable inside that
project's browser UI.

`engine.js` is a byte-for-byte port of chess.html's pure search/eval
functions — pulled out by name via `scripts/sync-engine.js`, the same
technique chess.html's own `_makeEngineWorker()` uses to build its Web
Worker source. `uci.js` is a thin protocol adapter on top: it translates UCI
text commands on stdin into calls to `engine.computerMove()` and writes UCI
replies to stdout. No other logic is duplicated or reimplemented.

## Requirements

Plain Node.js (14+). No npm dependencies — `engine.js` and `uci.js` are
vanilla JS.

## Manual test

```
node uci.js
uci
isready
position startpos
go movetime 1000
quit
```

You should see `id name ...` / `uciok`, `readyok`, then an `info ...` /
`bestmove ...` line about a second after `go`.

Run the automated smoke test with `npm test`.

## Rating your engine locally with cutechess-cli

The most direct way to get a real Elo estimate: play a gauntlet against
opponents of known strength and compute the result with a proper rating
tool.

1. Install [cutechess-cli](https://github.com/cutechess/cutechess) and
   [Stockfish](https://stockfishchess.org/). On Windows, both are on winget:
   `winget install Stockfish.Stockfish` and `winget install CuteChess.CuteChess`
   (the latter bundles `cutechess-cli.exe` alongside the GUI).
2. Run `gauntlet/run-gauntlet.sh`. It pits `uci.js` against Stockfish at
   several `UCI_LimitStrength` levels (1500/1800/2100/2400 by default) using
   a 16-line opening book (`gauntlet/openings.pgn`) so games aren't
   repetitive, and writes a timestamped PGN to `gauntlet/results/`:

   ```
   STOCKFISH_CMD=/path/to/stockfish CUTECHESS_CMD=/path/to/cutechess-cli \
     ./gauntlet/run-gauntlet.sh
   ```

   Tune it via env vars: `TC` (time control, default `10+0.1`), `ROUNDS`
   (per opponent, default 20 — each round is 2 games with colors swapped),
   `CONCURRENCY` (default 4), `ELOS` (space-separated Stockfish levels).
3. Get an anchored Elo estimate from the PGN. If you have
   [`ordo`](https://github.com/michiguel/Ordo) or `bayeselo` built/installed,
   use those. Otherwise, `gauntlet/compute-elo.js` does the same thing with
   zero dependencies — a maximum-likelihood logistic fit of a single unknown
   rating (your engine) against the fixed, known ratings of the `SF-<elo>`
   opponents, with a 95% confidence interval:

   ```
   node gauntlet/compute-elo.js gauntlet/results/<file>.pgn ChessBotUCI \
     SF-1500=1500 SF-1800=1800 SF-2100=2100 SF-2400=2400
   ```

   Add `--exclude-time-forfeits` to drop games ended by `[Termination "time
   forfeit"]` from the fit. If the test machine was under load during the
   run (other concurrent engine processes, etc.), a flag on move 6 out of
   an opening book is a giveaway that a game was lost to scheduling
   contention rather than chess — worth checking for and excluding before
   trusting the number. In practice the effect is usually small (one run:
   2160±84 raw vs 2130±88 excluding 8/120 forfeited games) but it costs
   nothing to check.

   This is the same underlying model Elo/ordo/bayeselo all use (logistic
   win-probability regression) — it just skips ordo's Bayesian smoothing
   prior, which mainly matters when some pairings have very few games. A few
   hundred games against a spread of opponents (raise `ROUNDS` and/or
   `ELOS`) narrows the confidence interval a lot more than any single match
   would.

Equivalently, without the script — this is what it runs under the hood:

   ```
   cutechess-cli \
     -engine cmd=node arg="/path/to/chess-bot-uci/uci.js" name=ChessBotUCI \
     -engine cmd=stockfish option.UCI_LimitStrength=true option.UCI_Elo=1500 name=SF-1500 \
     -engine cmd=stockfish option.UCI_LimitStrength=true option.UCI_Elo=1800 name=SF-1800 \
     -each proto=uci tc=40/60+0.6 \
     -rounds 200 -repeat -concurrency 4 -pgnout games.pgn
   ```

3. Feed the results into [`ordo`](https://github.com/michiguel/Ordo) or
   `bayeselo` to get an Elo estimate with a confidence interval, anchored to
   your reference engines' known ratings. A few hundred games against a
   spread of opponents gives a much more reliable number than any single
   match.

## Submitting to CCRL / CEGT

These are the widely-cited "official" rating lists. They run your engine
themselves on standardized hardware against their existing pool, so no local
setup is needed beyond having a working UCI binary (this one) — but there
are submission queues, eligibility rules (deterministic play, no online
learning, open source, etc.), and it can take a while to get a result. See
each site's submission instructions:

- [CCRL](https://computerchess.org.uk/ccrl/)
- [CEGT](http://www.cegt.net/)

## Known limitations

- **`stop` is accepted but not preemptive.** The search
  (`engine.computerMove`) runs synchronously on Node's single thread, so a
  `stop` command sent mid-search can't interrupt it the way a threaded
  engine would — Node can't process stdin again until the current call
  returns. In practice this doesn't matter for gauntlet/tournament play:
  every `go` is already called with a bounded time or depth budget (derived
  from `movetime`/`wtime`+`btime`/`depth`) that the search self-enforces via
  its own internal deadline check. It only matters for `go infinite`
  analysis workflows, which aren't the target use case here.
- **No pondering.** `ponder` / `ponderhit` are accepted but ignored.
- **No configurable options.** `setoption` is accepted but ignored — there's
  currently nothing to tune (hash size, threads, etc. are fixed constants
  ported from chess.html).
- **Time management is a simple heuristic**, not tournament-tuned: for
  `wtime`/`btime`, it allocates `remaining / movesToGo + 80% of increment`,
  capped at half the remaining clock. Safe against flagging, but not
  optimized for maximum strength under a clock.

## Keeping this in sync with chess.html

If the parent project's engine changes, regenerate `engine.js`:

```
node scripts/sync-engine.js /path/to/chess.html
```

This re-extracts the same named functions/constants and overwrites
`engine.js`. Review the diff before committing — a renamed or restructured
function in chess.html will make extraction fail loudly (function/const not
found) rather than silently drift out of sync.
