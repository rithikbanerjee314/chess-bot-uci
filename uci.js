#!/usr/bin/env node
'use strict';
// UCI (Universal Chess Interface) wrapper around engine.js.
//
// engine.js is a byte-for-byte port of the chess.html browser app's pure
// search/eval functions (see scripts/sync-engine.js). This file's only job
// is protocol translation: read UCI text commands on stdin, drive the
// engine, write UCI replies on stdout — so this binary can be pointed at by
// any standard UCI tool (cutechess-cli, Arena, a CCRL/CEGT submission, a
// Lichess bot bridge, etc.) for engine-strength testing.

const engine = require('./engine.js');

const ENGINE_NAME = 'ChessBotUCI 0.1.0';
const ENGINE_AUTHOR = 'Rithik Banerjee';

// Sane defaults so `go` never runs unbounded when a GUI omits time controls.
const DEFAULT_MAX_DEPTH = 40;   // well under engine.js's MAX_PLY=64 killer-table size
const DEFAULT_GO_MS = 1000;     // bare `go` with no params at all
const INFINITE_GO_MS = 10 * 60 * 1000; // pragmatic ceiling for `go infinite` — see README

let pos = null; // { board, turn, enPassant, castling }

function setStartpos() {
  const p = engine.parseFen(engine.INIT_FEN);
  pos = { board: p.board, turn: p.turn, enPassant: p.enPassant, castling: p.castling };
}

function setFen(fenTokens) {
  const p = engine.parseFen(fenTokens.join(' '));
  pos = { board: p.board, turn: p.turn, enPassant: p.enPassant, castling: p.castling };
}

function applyUciMove(moveStr) {
  const from = engine.algebraicToIndex(moveStr.slice(0, 2));
  const to = engine.algebraicToIndex(moveStr.slice(2, 4));
  const promo = moveStr.length > 4 ? moveStr[4].toLowerCase() : null;
  const legal = engine.legalMoves(pos.board, pos.turn, pos.enPassant, pos.castling);
  const match = legal.find(m => m.from === from && m.to === to && m.promo === promo);
  if (!match) throw new Error('illegal move in position command: ' + moveStr);
  const { board, newEP, newCastling } = engine.applyMove(pos.board, from, to, promo, pos.enPassant, pos.castling);
  pos = { board, turn: pos.turn === 'w' ? 'b' : 'w', enPassant: newEP, castling: newCastling };
}

function moveToUci(mv) {
  let s = engine.indexToAlgebraic(mv.from) + engine.indexToAlgebraic(mv.to);
  if (mv.promo) s += mv.promo;
  return s;
}

function handlePosition(tokens) {
  let idx = 0;
  if (tokens[idx] === 'startpos') {
    setStartpos();
    idx++;
  } else if (tokens[idx] === 'fen') {
    idx++;
    const fenTokens = [];
    while (idx < tokens.length && tokens[idx] !== 'moves') { fenTokens.push(tokens[idx]); idx++; }
    setFen(fenTokens);
  } else {
    return; // malformed `position` command — ignore per UCI convention
  }
  if (tokens[idx] === 'moves') {
    idx++;
    for (; idx < tokens.length; idx++) applyUciMove(tokens[idx]);
  }
}

// Turns `go` parameters into { maxDepth, searchMs } for engine.computerMove.
// wtime/btime uses a simple time-management heuristic (remaining/movesToGo +
// 80% of increment, capped at half the remaining clock) — not tournament-
// tuned, but safe against flagging. See README for known limitations.
function computeSearchBudget(params, turn) {
  if (params.depth) return { maxDepth: params.depth, searchMs: 24 * 60 * 60 * 1000 };
  if (params.movetime) return { maxDepth: DEFAULT_MAX_DEPTH, searchMs: Math.max(50, params.movetime - 20) };
  if (params.wtime != null || params.btime != null) {
    const myTime = (turn === 'w' ? params.wtime : params.btime) || 0;
    const myInc = (turn === 'w' ? params.winc : params.binc) || 0;
    const movesToGo = params.movestogo || 30;
    let allocated = myTime / movesToGo + myInc * 0.8;
    allocated = Math.min(allocated, myTime * 0.5);
    allocated = Math.max(allocated, 50);
    return { maxDepth: DEFAULT_MAX_DEPTH, searchMs: Math.floor(allocated) };
  }
  if (params.infinite) return { maxDepth: DEFAULT_MAX_DEPTH, searchMs: INFINITE_GO_MS };
  return { maxDepth: 12, searchMs: DEFAULT_GO_MS };
}

function handleGo(tokens) {
  const params = {};
  const NUMERIC = new Set(['wtime', 'btime', 'winc', 'binc', 'movetime', 'depth', 'movestogo', 'nodes', 'mate']);
  for (let i = 0; i < tokens.length; i++) {
    const t = tokens[i];
    if (NUMERIC.has(t)) params[t] = parseInt(tokens[++i], 10);
    else if (t === 'infinite') params.infinite = true;
    else if (t === 'ponder') params.ponder = true;
  }

  const { maxDepth, searchMs } = computeSearchBudget(params, pos.turn);
  const result = engine.computerMove({
    board: pos.board, turn: pos.turn, enPassant: pos.enPassant, castling: pos.castling,
    maxDepth, searchMs,
  });

  if (!result) {
    // No legal moves (checkmate/stalemate). UCI has no dedicated "no move"
    // reply; send the conventional null move so the GUI doesn't hang.
    send('bestmove 0000');
    return;
  }

  const stats = engine.getSearchStats();
  const scoreStm = pos.turn === 'w' ? stats.score : -stats.score; // UCI wants side-to-move POV
  send(`info depth ${stats.depth} score cp ${scoreStm} nodes ${stats.nodes}`);
  send('bestmove ' + moveToUci(result));
}

function send(line) { process.stdout.write(line + '\n'); }

setStartpos();

const readline = require('readline');
const rl = readline.createInterface({ input: process.stdin, terminal: false });

rl.on('line', (raw) => {
  const line = raw.trim();
  if (!line) return;
  const tokens = line.split(/\s+/);
  const cmd = tokens[0];
  try {
    switch (cmd) {
      case 'uci':
        send('id name ' + ENGINE_NAME);
        send('id author ' + ENGINE_AUTHOR);
        send('uciok');
        break;
      case 'isready':
        send('readyok');
        break;
      case 'ucinewgame':
        engine.ttClear();
        engine.clearHistory();
        setStartpos();
        break;
      case 'position':
        handlePosition(tokens.slice(1));
        break;
      case 'go':
        handleGo(tokens.slice(1));
        break;
      case 'stop':
        // Best-effort only: the search is synchronous, so `stop` can't
        // preempt an in-progress `go` the way a threaded engine would — it's
        // accepted here purely for protocol compliance. In practice this
        // doesn't affect gauntlet play, since `go` is always called with a
        // bounded time/depth budget that the search already self-enforces.
        // See README "Known limitations."
        break;
      case 'setoption':
      case 'debug':
      case 'ponderhit':
        break; // no tunable options exposed yet — accepted and ignored
      case 'quit':
        process.exit(0);
        break;
      default:
        break; // unknown command — UCI engines are expected to ignore these
    }
  } catch (err) {
    process.stderr.write('[uci] error handling "' + line + '": ' + (err && err.stack || err) + '\n');
  }
});

process.stdin.on('end', () => process.exit(0));
