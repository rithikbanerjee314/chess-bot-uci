'use strict';
// Post-mortem stats from a cutechess-cli gauntlet PGN: per-player search
// depth/time, termination reasons, and — for the subject engine's losses —
// game length and the move where its own eval first collapsed. Reads the
// {eval/depth time} comments cutechess records for every non-book move.
//
// Usage: node analyze-pgn.js <pgn-file> <subject-name>

const fs = require('fs');

const [, , pgnPath, subject] = process.argv;
if (!pgnPath || !subject) {
  console.error('Usage: node analyze-pgn.js <pgn> <subjectName>');
  process.exit(1);
}

const pgn = fs.readFileSync(pgnPath, 'utf8');
const games = pgn.split(/\n\n(?=\[Event)/).filter(Boolean);

function tag(g, name) {
  const m = new RegExp('\\[' + name + ' "([^"]*)"\\]').exec(g);
  return m ? m[1] : null;
}

// Moves alternate white/black. Each annotated move looks like:
//   Nf3 {+0.31/14 0.68s}   or   e4 {book}
// Mate scores look like {+M12/9 0.5s}.
const MOVE_RE = /\{([+-]?(?:\d+\.\d+|M\d+))\/(\d+) ([\d.]+)s\}|\{book\}/g;

const perOpp = {}; // opp -> stats
let subjDepths = [], subjTimes = [];
let oppDepths = [];
const lossLengths = [], winLengths = [];
const lossCollapseMove = []; // subject's move number where its own eval first went <= -3.00
let flagLosses = 0;

for (const g of games) {
  const w = tag(g, 'White'), b = tag(g, 'Black'), r = tag(g, 'Result');
  const termination = tag(g, 'Termination');
  const plyCount = Number(tag(g, 'PlyCount') || 0);
  if (w !== subject && b !== subject) continue;
  const subjIsWhite = w === subject;
  const opp = subjIsWhite ? b : w;
  const subjScore = r === '1-0' ? (subjIsWhite ? 1 : 0)
                  : r === '0-1' ? (subjIsWhite ? 0 : 1) : 0.5;

  if (termination && /time/i.test(termination) && subjScore === 0) flagLosses++;

  // The movetext is everything after the last tag line.
  const body = g.slice(g.lastIndexOf(']') + 1);
  let m, plyIdx = 0;
  MOVE_RE.lastIndex = 0;
  let firstCollapse = null;
  while ((m = MOVE_RE.exec(body)) !== null) {
    const isSubjMove = (plyIdx % 2 === 0) === subjIsWhite;
    if (m[1] !== undefined) {
      const evalStr = m[1], depth = Number(m[2]), time = Number(m[3]);
      if (isSubjMove) {
        subjDepths.push(depth);
        subjTimes.push(time);
        const cp = evalStr.includes('M')
          ? (evalStr.startsWith('-') ? -9999 : 9999)
          : Number(evalStr) * 100;
        if (firstCollapse === null && cp <= -300) firstCollapse = Math.floor(plyIdx / 2) + 1;
      } else {
        oppDepths.push(depth);
      }
    }
    plyIdx++;
  }

  if (!perOpp[opp]) perOpp[opp] = { games: 0, score: 0 };
  perOpp[opp].games++;
  perOpp[opp].score += subjScore;

  if (subjScore === 0) {
    lossLengths.push(plyCount);
    if (firstCollapse !== null) lossCollapseMove.push(firstCollapse);
  } else if (subjScore === 1) {
    winLengths.push(plyCount);
  }
}

function stats(arr) {
  if (!arr.length) return { n: 0 };
  const sorted = [...arr].sort((a, b) => a - b);
  const sum = arr.reduce((a, b) => a + b, 0);
  return {
    n: arr.length,
    mean: (sum / arr.length).toFixed(2),
    median: sorted[Math.floor(arr.length / 2)],
    p10: sorted[Math.floor(arr.length * 0.1)],
    p90: sorted[Math.floor(arr.length * 0.9)],
  };
}

console.log(`Subject: ${subject}`);
console.log('');
console.log('Search depth per move:   ', JSON.stringify(stats(subjDepths)));
console.log('Opponent depth per move: ', JSON.stringify(stats(oppDepths)));
console.log('Time per move (s):       ', JSON.stringify(stats(subjTimes)));
console.log('');
console.log(`Losses on time: ${flagLosses}`);
console.log('Loss game length (plies):', JSON.stringify(stats(lossLengths)));
console.log('Win game length (plies): ', JSON.stringify(stats(winLengths)));
console.log('Move where own eval first hit -3.00 in losses:', JSON.stringify(stats(lossCollapseMove)));
console.log('');
for (const [opp, s] of Object.entries(perOpp)) {
  console.log(`vs ${opp}: ${s.score}/${s.games}`);
}
