'use strict';
// End-to-end smoke test: drives uci.js as a real child process through a
// UCI handshake plus a couple of `go` searches, and sanity-checks engine.js
// directly. Not a full test suite — just enough to catch a broken sync
// (e.g. scripts/sync-engine.js pulling in a renamed/changed function) before
// it reaches a rating gauntlet.

const { spawn } = require('child_process');
const path = require('path');
const assert = require('assert');
const engine = require('../engine.js');

function fail(msg) {
  console.error('FAIL:', msg);
  process.exitCode = 1;
}

// ---- Direct engine.js sanity checks ----
const start = engine.parseFen(engine.INIT_FEN);
const legal = engine.legalMoves(start.board, start.turn, start.enPassant, start.castling);
assert.strictEqual(legal.length, 20, 'startpos should have 20 legal moves, got ' + legal.length);
assert.strictEqual(engine.evaluateBoard(start.board), 0, 'symmetric startpos should evaluate to 0');
console.log('engine.js direct checks passed (20 legal moves, eval=0 at startpos)');

// ---- UCI protocol end-to-end ----
const uciPath = path.join(__dirname, '..', 'uci.js');
const child = spawn(process.execPath, [uciPath], { stdio: ['pipe', 'pipe', 'pipe'] });

let buffer = '';
const seen = { uciok: false, readyok: false, bestmoveCount: 0 };
let lastBestmove = null;

child.stdout.on('data', (chunk) => {
  buffer += chunk.toString();
  let idx;
  while ((idx = buffer.indexOf('\n')) !== -1) {
    const line = buffer.slice(0, idx).trim();
    buffer = buffer.slice(idx + 1);
    if (!line) continue;
    if (line === 'uciok') seen.uciok = true;
    if (line === 'readyok') seen.readyok = true;
    if (line.startsWith('bestmove')) {
      seen.bestmoveCount++;
      lastBestmove = line.split(/\s+/)[1];
    }
  }
});

child.stderr.on('data', (chunk) => {
  console.error('[uci stderr]', chunk.toString());
});

function send(line) { child.stdin.write(line + '\n'); }

send('uci');
send('isready');
send('ucinewgame');
send('position startpos');
send('go depth 4');

setTimeout(() => {
  send('position startpos moves e2e4 e7e5');
  send('go movetime 500');
}, 500);

setTimeout(() => {
  send('quit');
}, 1500);

child.on('close', (code) => {
  if (!seen.uciok) fail('never received uciok');
  if (!seen.readyok) fail('never received readyok');
  if (seen.bestmoveCount !== 2) fail('expected 2 bestmove replies, got ' + seen.bestmoveCount);
  if (lastBestmove && !/^[a-h][1-8][a-h][1-8][qrbn]?$/.test(lastBestmove)) {
    fail('last bestmove "' + lastBestmove + '" is not well-formed UCI notation');
  }
  if (process.exitCode) {
    console.error('SMOKE TEST FAILED');
  } else {
    console.log('SMOKE TEST PASSED (uciok, readyok, ' + seen.bestmoveCount + ' bestmove replies, last=' + lastBestmove + ')');
  }
  process.exit(process.exitCode || 0);
});
