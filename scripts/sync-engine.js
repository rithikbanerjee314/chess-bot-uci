'use strict';
// Regenerates engine.js by extracting the pure engine functions and constant
// tables out of the parent project's chess.html by name — the same technique
// chess.html's own _makeEngineWorker() uses to build its Web Worker source.
//
// This keeps the UCI engine byte-for-byte identical to whatever plays in the
// browser. Re-run this whenever the parent project's engine changes:
//
//   node scripts/sync-engine.js /path/to/chess.html
//
// engine.js is committed to this repo as plain generated source — this repo
// has no runtime dependency on chess.html existing anywhere.

const fs = require('fs');
const path = require('path');

const srcPath = process.argv[2];
if (!srcPath) {
  console.error('Usage: node scripts/sync-engine.js /path/to/chess.html');
  process.exit(1);
}
const outPath = path.join(__dirname, '..', 'engine.js');
const src = fs.readFileSync(srcPath, 'utf8');

function findMatchingBracket(text, openIdx, openCh, closeCh) {
  let depth = 0;
  let inString = null;
  let inLineComment = false;
  let inBlockComment = false;
  for (let i = openIdx; i < text.length; i++) {
    const c = text[i];
    const prev = text[i - 1];
    if (inLineComment) { if (c === '\n') inLineComment = false; continue; }
    if (inBlockComment) { if (c === '/' && prev === '*') inBlockComment = false; continue; }
    if (inString) {
      if (c === '\\') { i++; continue; }
      if (c === inString) inString = null;
      continue;
    }
    if (c === '/' && text[i + 1] === '/') { inLineComment = true; continue; }
    if (c === '/' && text[i + 1] === '*') { inBlockComment = true; continue; }
    if (c === '"' || c === "'" || c === '`') { inString = c; continue; }
    if (c === openCh) depth++;
    else if (c === closeCh) {
      depth--;
      if (depth === 0) return i;
    }
  }
  throw new Error('unbalanced brackets starting at ' + openIdx);
}

function extractFunction(name) {
  const re = new RegExp(`\\nfunction\\s+${name}\\s*\\(`);
  const m = re.exec(src);
  if (!m) throw new Error('function not found: ' + name);
  const startIdx = m.index + 1;
  const braceStart = src.indexOf('{', startIdx);
  const braceEnd = findMatchingBracket(src, braceStart, '{', '}');
  return src.slice(startIdx, braceEnd + 1);
}

function extractConst(name) {
  const re = new RegExp(`\\nconst\\s+${name}\\s*=\\s*`);
  const m = re.exec(src);
  if (!m) throw new Error('const not found: ' + name);
  const startIdx = m.index + 1;
  const afterEq = src.indexOf('=', startIdx) + 1;
  let i = afterEq;
  while (/\s/.test(src[i])) i++;
  const openCh = src[i];
  let semi;
  if (openCh === '[' || openCh === '{') {
    const closeCh = openCh === '[' ? ']' : '}';
    const endIdx = findMatchingBracket(src, i, openCh, closeCh);
    semi = src.indexOf(';', endIdx);
  } else {
    semi = src.indexOf(';', i);
  }
  return src.slice(startIdx, semi + 1);
}

// Same lists chess.html's _makeEngineWorker() uses for its Worker source,
// plus a handful of extras (FEN parsing, notation) this standalone CLI needs
// that the in-browser Worker doesn't (the main thread already parses FEN).
const CONST_NAMES = [
  'PHASE_WEIGHTS', 'MAX_PHASE', 'PV_MG', 'PV_EG',
  'PAWN_MG_PST', 'PAWN_EG_PST', 'KNIGHT_MG_PST', 'KNIGHT_EG_PST',
  'BISHOP_PST', 'ROOK_MG_PST', 'ROOK_EG_PST', 'QUEEN_PST',
  'KING_MG_PST', 'KING_EG_PST', 'PASSED_MG', 'PASSED_EG',
  'THREAT_BY_PAWN_MG', 'THREAT_BY_PAWN_EG', 'ATTACK_UNIT',
  'TEMPO_BONUS', 'MATE_SCORE', 'QMAX',
  'TT_EXACT', // statement also declares TT_LOWER, TT_UPPER via comma
  'TT_SIZE', 'MAX_PLY', '_pieceIdx',
  'INIT_FEN',
];

const FUNC_NAMES = [
  'pieceColor', 'pieceType', 'isEnemy', 'isFriend',
  'pseudoMoves', 'isSquareAttacked', 'findKing', 'isInCheck',
  'applyMove', 'legalMoves',
  'gamePhase', 'taper', 'mirrorSq', 'getPST',
  'evalRooksMGEG', 'evalKnightOutpostsMGEG', 'evalThreatsMGEG',
  'evalSpaceMG', 'kingZoneSafety',
  'phase128', 'scaleFactor',
  'pawnStructureMGEG', 'mobilityMGEG', 'evaluateMGEG', 'evaluateBoard',
  'fastEval', 'evaluateBoardLazy',
  'leastValuableAttacker', 'seeCapture',
  'positionHash', 'historyIdx',
  'moveOrderScoreEx', 'orderMovesEx',
  'quiesce', 'negamax', 'hasNonPawnMaterial',
  'computerMove', 'searchEval',
  'parseFen', 'algebraicToIndex', 'indexToAlgebraic', 'moveToNotation',
];

const constSrc = CONST_NAMES.map(extractConst).join('\n');
const funcSrc = FUNC_NAMES.map(extractFunction).join('\n\n');

const stateSrc = `
// Mutable per-process search state + TT/history helpers, ported verbatim
// from chess.html's _makeEngineWorker stateSrc template.
const PIECE_VALUES = PV_MG;
let _tt = new Array(TT_SIZE).fill(null);
function ttProbe(hash) {
  const e = _tt[hash & (TT_SIZE - 1)];
  return (e && e.hash === hash) ? e : null;
}
function ttStore(hash, depth, score, bound, move) {
  const slot = hash & (TT_SIZE - 1);
  const ex = _tt[slot];
  if (!ex || ex.depth <= depth) _tt[slot] = { hash, depth, score, bound, move };
}
function ttClear() { _tt = new Array(TT_SIZE).fill(null); }
let _killers = Array.from({ length: MAX_PLY }, () => [null, null]);
let _history = new Int32Array(64 * 12);
function clearHistory() {
  _killers = Array.from({ length: MAX_PLY }, () => [null, null]);
  _history = new Int32Array(64 * 12);
}
let _searchDeadline = Infinity;
let _nodeCount = 0;
let lastSearchScore = 0;
let lastSearchDepth = 0;
let _prevKeys = new Set();
let _pathKeys = [];
function getSearchStats() { return { score: lastSearchScore, depth: lastSearchDepth, nodes: _nodeCount }; }
`;

const header = `'use strict';
// AUTO-EXTRACTED from chess.html — DO NOT hand-edit.
// Ported verbatim (byte-for-byte function bodies) from the parent chess
// project's pure, closure-free engine functions so this UCI engine plays
// identically to the browser app. Re-run scripts/sync-engine.js against an
// updated chess.html to refresh this file.
`;

const footer = `
module.exports = {
  INIT_FEN, parseFen, algebraicToIndex, indexToAlgebraic, moveToNotation,
  pieceColor, pieceType, isEnemy, isFriend,
  pseudoMoves, isSquareAttacked, findKing, isInCheck,
  applyMove, legalMoves,
  evaluateBoard,
  computerMove, searchEval,
  positionHash, ttClear, clearHistory,
  getSearchStats,
};
`;

const out = header + '\n' + constSrc + '\n' + stateSrc + '\n' + funcSrc + '\n' + footer;
fs.writeFileSync(outPath, out);
console.log('Wrote', outPath, out.length, 'bytes');
console.log('Consts:', CONST_NAMES.length, 'Funcs:', FUNC_NAMES.length);
