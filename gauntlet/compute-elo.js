'use strict';
// Anchored Elo estimate from a gauntlet PGN: fits a single unknown rating
// (the subject engine) against N opponents whose ratings are fixed/known,
// via logistic-regression maximum likelihood — the same model Elo/ordo/
// bayeselo all build on, just without ordo's Bayesian smoothing prior.
//
// Usage: node compute-elo.js <pgn-file> <subject-name> <opp1>=<elo1> [<opp2>=<elo2> ...] [--exclude-time-forfeits]
//
// --exclude-time-forfeits drops games with [Termination "time forfeit"].
// Useful when the test machine was under load during the run (e.g. other
// concurrent engine processes) — a flag on move 6 out of an opening book is
// a giveaway that a game was lost to scheduling contention, not chess, and
// including it as a normal loss would bias the estimate.

const fs = require('fs');

const rawArgs = process.argv.slice(2);
const excludeTimeForfeits = rawArgs.includes('--exclude-time-forfeits');
const [pgnPath, subject, ...anchorArgs] = rawArgs.filter(a => a !== '--exclude-time-forfeits');
if (!pgnPath || !subject || anchorArgs.length === 0) {
  console.error('Usage: node compute-elo.js <pgn> <subjectName> <opp>=<elo> [...] [--exclude-time-forfeits]');
  process.exit(1);
}
const anchors = {};
for (const a of anchorArgs) {
  const [name, elo] = a.split('=');
  anchors[name] = Number(elo);
}

const pgn = fs.readFileSync(pgnPath, 'utf8');
const games = pgn.split(/\n\n(?=\[Event)/).filter(Boolean);

// results[opponentName] = { games: n, score: sum of subject's points }
const results = {};
let excludedCount = 0;
for (const g of games) {
  const white = /\[White "([^"]+)"\]/.exec(g);
  const black = /\[Black "([^"]+)"\]/.exec(g);
  const result = /\[Result "([^"]+)"\]/.exec(g);
  if (!white || !black || !result) continue;
  const w = white[1], b = black[1], r = result[1];
  let opp, subjIsWhite;
  if (w === subject) { opp = b; subjIsWhite = true; }
  else if (b === subject) { opp = w; subjIsWhite = false; }
  else continue;
  if (!(opp in anchors)) continue;

  if (excludeTimeForfeits && /\[Termination "time forfeit"\]/.test(g)) {
    excludedCount++;
    continue;
  }

  let score;
  if (r === '1-0') score = subjIsWhite ? 1 : 0;
  else if (r === '0-1') score = subjIsWhite ? 0 : 1;
  else if (r === '1/2-1/2') score = 0.5;
  else continue; // unfinished/unknown result

  if (!results[opp]) results[opp] = { games: 0, score: 0 };
  results[opp].games += 1;
  results[opp].score += score;
}
if (excludeTimeForfeits) console.log(`Excluded ${excludedCount} time-forfeit game(s).\n`);

console.log('Parsed results (subject:', subject + '):');
for (const [opp, r] of Object.entries(results)) {
  console.log(`  vs ${opp} (Elo ${anchors[opp]}): ${r.score}/${r.games} = ${(100 * r.score / r.games).toFixed(1)}%`);
}

function expectedScore(rSubject, rOpp) {
  return 1 / (1 + Math.pow(10, (rOpp - rSubject) / 400));
}

// Total observed score and total games across all anchored opponents.
function totalDelta(rSubject) {
  let delta = 0;
  for (const [opp, r] of Object.entries(results)) {
    const e = expectedScore(rSubject, anchors[opp]);
    delta += r.score - e * r.games;
  }
  return delta;
}

// Bisection for the MLE root: sum(actual - expected) = 0.
let lo = 0, hi = 3500;
for (let i = 0; i < 200; i++) {
  const mid = (lo + hi) / 2;
  if (totalDelta(mid) > 0) lo = mid; else hi = mid;
}
const rHat = (lo + hi) / 2;

// Standard error via observed Fisher information:
// Var(R) ≈ 1 / sum_i [ n_i * e_i * (1-e_i) * (ln10/400)^2 ]
const k = Math.log(10) / 400;
let fisherInfo = 0;
let totalGames = 0, totalScore = 0;
for (const [opp, r] of Object.entries(results)) {
  const e = expectedScore(rHat, anchors[opp]);
  fisherInfo += r.games * e * (1 - e) * k * k;
  totalGames += r.games;
  totalScore += r.score;
}
const se = 1 / Math.sqrt(fisherInfo);

console.log('');
console.log(`Total: ${totalScore}/${totalGames} = ${(100 * totalScore / totalGames).toFixed(1)}% across all anchored opponents`);
console.log('');
console.log(`Anchored Elo estimate for ${subject}: ${rHat.toFixed(0)} +/- ${(1.96 * se).toFixed(0)} (95% CI)`);
console.log(`(maximum-likelihood logistic fit against fixed opponent ratings ${JSON.stringify(anchors)})`);
