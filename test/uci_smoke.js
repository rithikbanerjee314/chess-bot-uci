'use strict';
// End-to-end smoke test for the C++ engine: drives the compiled binary as a
// real child process through a UCI handshake plus a couple of `go`
// searches, keeping stdin open between commands (closing it immediately
// after `go` triggers the engine's EOF-cleanup `stop`, cutting the search
// short — this mirrors how a real GUI behaves: it never closes the pipe
// mid-search).
//
// Usage: node test/uci_smoke.js [path-to-exe]

const { spawn } = require('child_process');
const path = require('path');
const assert = require('assert');

const exePath = process.argv[2] || path.join(__dirname, '..', 'build', 'bin', 'atlas.exe');

function fail(msg) {
  console.error('FAIL:', msg);
  process.exitCode = 1;
}

const child = spawn(exePath, [], { stdio: ['pipe', 'pipe', 'pipe'] });

let buffer = '';
const seen = { uciok: false, readyok: false, bestmoveCount: 0 };
const bestmoves = [];
const infoDepths = [];

child.stdout.on('data', (chunk) => {
  buffer += chunk.toString();
  let idx;
  while ((idx = buffer.indexOf('\n')) !== -1) {
    const line = buffer.slice(0, idx).trim();
    buffer = buffer.slice(idx + 1);
    if (!line) continue;
    if (line === 'uciok') seen.uciok = true;
    if (line === 'readyok') seen.readyok = true;
    if (line.startsWith('info depth')) {
      const m = /depth (\d+)/.exec(line);
      if (m) infoDepths.push(Number(m[1]));
    }
    if (line.startsWith('bestmove')) {
      seen.bestmoveCount++;
      bestmoves.push(line.split(/\s+/)[1]);
    }
  }
});
child.stderr.on('data', (chunk) => console.error('[stderr]', chunk.toString()));

function send(line) { child.stdin.write(line + '\n'); }
function wait(ms) { return new Promise((r) => setTimeout(r, ms)); }

(async () => {
  send('uci');
  send('isready');
  await wait(200);

  send('ucinewgame');
  send('position startpos');
  send('go depth 6');
  await wait(3000); // give a fixed-depth search real time to finish on its own

  send('position startpos moves e2e4 e7e5');
  send('go movetime 800');
  await wait(1500); // must exceed movetime so the engine's own deadline (not our stdin close) ends it

  send('quit');
  await wait(200);
  child.kill();

  if (!seen.uciok) fail('never received uciok');
  if (!seen.readyok) fail('never received readyok');
  if (seen.bestmoveCount !== 2) fail('expected 2 bestmove replies, got ' + seen.bestmoveCount);
  for (const bm of bestmoves) {
    if (!/^[a-h][1-8][a-h][1-8][qrbn]?$/.test(bm) && bm !== '0000') {
      fail('bestmove "' + bm + '" is not well-formed UCI notation');
    }
  }
  if (infoDepths.length && Math.max(...infoDepths) < 3) {
    fail('search never got past depth ' + Math.max(...infoDepths) + ' — likely stopping prematurely');
  }

  if (process.exitCode) {
    console.error('UCI SMOKE TEST FAILED');
  } else {
    console.log('UCI SMOKE TEST PASSED (uciok, readyok, ' + seen.bestmoveCount +
      ' bestmove replies: ' + bestmoves.join(', ') + '; depths reached: ' + infoDepths.join(',') + ')');
  }
  process.exit(process.exitCode || 0);
})();
