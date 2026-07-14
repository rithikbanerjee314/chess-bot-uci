'use strict';
// AUTO-EXTRACTED from chess.html — DO NOT hand-edit.
// Ported verbatim (byte-for-byte function bodies) from the parent chess
// project's pure, closure-free engine functions so this UCI engine plays
// identically to the browser app. Re-run scripts/sync-engine.js against an
// updated chess.html to refresh this file.

const PHASE_WEIGHTS = { n: 1, b: 1, r: 2, q: 4 };
const MAX_PHASE = 24;
const PV_MG = { p: 82,  n: 337, b: 365, r: 477, q: 1025, k: 20000 };
const PV_EG = { p: 94,  n: 281, b: 297, r: 512, q: 936,  k: 20000 };
const PAWN_MG_PST = [
   0,  0,  0,  0,  0,  0,  0,  0,
   3,  6,  6,-16,-16,  6,  6,  3,
   2, -3,  2,  3,  3,  2, -3,  2,
  -2, -2,  3, 28, 28,  3, -2, -2,
   3,  3, 12, 36, 36, 12,  3,  3,
   8,  8, 17, 24, 24, 17,  8,  8,
  46, 46, 46, 46, 46, 46, 46, 46,
   0,  0,  0,  0,  0,  0,  0,  0,
];
const PAWN_EG_PST = [
   0,  0,  0,  0,  0,  0,  0,  0,
  -8, -8, -8, -8, -8, -8, -8, -8,
  -5, -5, -5, -5, -5, -5, -5, -5,
   2,  2,  2,  2,  2,  2,  2,  2,
  14, 14, 14, 14, 14, 14, 14, 14,
  28, 28, 28, 28, 28, 28, 28, 28,
  50, 50, 50, 50, 50, 50, 50, 50,
   0,  0,  0,  0,  0,  0,  0,  0,
];
const KNIGHT_MG_PST = [
 -50,-40,-30,-30,-30,-30,-40,-50,
 -40,-20,  0,  5,  5,  0,-20,-40,
 -30,  5, 10, 15, 15, 10,  5,-30,
 -30,  0, 15, 20, 20, 15,  0,-30,
 -30,  5, 15, 20, 20, 15,  5,-30,
 -30,  0, 10, 15, 15, 10,  0,-30,
 -40,-20,  0,  5,  5,  0,-20,-40,
 -50,-40,-30,-30,-30,-30,-40,-50,
];
const KNIGHT_EG_PST = [
 -60,-40,-25,-25,-25,-25,-40,-60,
 -40,-25, -5,  0,  0, -5,-25,-40,
 -25, -5, 15, 18, 18, 15, -5,-25,
 -25, -5, 18, 23, 23, 18, -5,-25,
 -25, -5, 18, 23, 23, 18, -5,-25,
 -25, -5, 15, 18, 18, 15, -5,-25,
 -40,-25, -5,  0,  0, -5,-25,-40,
 -60,-40,-25,-25,-25,-25,-40,-60,
];
const BISHOP_PST = [
 -20,-10,-10,-10,-10,-10,-10,-20,
 -10,  5,  0,  0,  0,  0,  5,-10,
 -10,  8, 10, 10, 10, 10,  8,-10,
 -10,  0, 10, 10, 10, 10,  0,-10,
 -10,  5,  5, 10, 10,  5,  5,-10,
 -10,  0,  5, 10, 10,  5,  0,-10,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -20,-10,-10,-10,-10,-10,-10,-20,
];
const ROOK_MG_PST = [
   0,  0,  0,  5,  5,  0,  0,  0,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
  -5,  0,  0,  0,  0,  0,  0, -5,
   5, 10, 10, 10, 10, 10, 10,  5,
   0,  0,  0,  5,  5,  0,  0,  0,
];
const ROOK_EG_PST = [
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   3,  3,  3,  3,  3,  3,  3,  3,
   0,  0,  0,  0,  0,  0,  0,  0,
];
const QUEEN_PST = [
 -20,-10,-10, -5, -5,-10,-10,-20,
 -10,  0,  5,  0,  0,  5,  0,-10,
 -10,  5,  5,  5,  5,  5,  5,-10,
   0,  0,  5,  5,  5,  5,  0, -5,
  -5,  0,  5,  5,  5,  5,  0, -5,
 -10,  0,  5,  5,  5,  5,  0,-10,
 -10,  0,  0,  0,  0,  0,  0,-10,
 -20,-10,-10, -5, -5,-10,-10,-20,
];
const KING_MG_PST = [
  20, 30, 10,  0,  0, 10, 30, 20,
  20, 20,  0,  0,  0,  0, 20, 20,
 -10,-20,-20,-20,-20,-20,-20,-10,
 -20,-30,-30,-40,-40,-30,-30,-20,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
 -30,-40,-40,-50,-50,-40,-40,-30,
];
const KING_EG_PST = [
 -50,-40,-30,-20,-20,-30,-40,-50,
 -30,-20,-10,  0,  0,-10,-20,-30,
 -30,-10, 20, 30, 30, 20,-10,-30,
 -30,-10, 30, 40, 40, 30,-10,-30,
 -30,-10, 30, 40, 40, 30,-10,-30,
 -30,-10, 20, 30, 30, 20,-10,-30,
 -30,-30,  0,  0,  0,  0,-30,-30,
 -50,-30,-30,-30,-30,-30,-30,-50,
];
const PASSED_MG = [0, 0,  7, 16, 17, 64, 170, 0];
const PASSED_EG = [0, 0, 27, 32, 41, 72, 177, 0];
const THREAT_BY_PAWN_MG    = { n: 48, b: 48, r: 70, q: 90 };
const THREAT_BY_PAWN_EG    = { n: 38, b: 38, r: 50, q: 60 };
const ATTACK_UNIT = { n: 20, b: 20, r: 40, q: 80 };
const TEMPO_BONUS = 28;
const MATE_SCORE = 100000;
const QMAX = 6;
const TT_EXACT = 0, TT_LOWER = 1, TT_UPPER = 2;
const TT_SIZE  = 1 << 18;
const MAX_PLY = 64;
const _pieceIdx = { P:0, N:1, B:2, R:3, Q:4, K:5, p:6, n:7, b:8, r:9, q:10, k:11 };
const INIT_FEN = 'rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1';

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
function getSearchStats() { return { score: lastSearchScore, depth: lastSearchDepth, nodes: _nodeCount }; }

function pieceColor(p) {
  if (!p) return null;
  return p === p.toUpperCase() ? 'w' : 'b';
}

function pieceType(p) { return p ? p.toLowerCase() : null; }

function isEnemy(p, turn) { return p && pieceColor(p) !== turn; }

function isFriend(p, turn) { return p && pieceColor(p) === turn; }

function pseudoMoves(board, from, turn, enPassant, castling) {
  const p = board[from];
  if (!p || pieceColor(p) !== turn) return [];
  const type = pieceType(p);
  const moves = [];
  const rank = Math.floor(from/8), file = from%8;

  const addIfValid = (to) => {
    if (to < 0 || to > 63) return;
    if (!isFriend(board[to], turn)) moves.push(to);
  };

  const slide = (drs) => {
    for (const [dr, df] of drs) {
      let r = rank+dr, f = file+df;
      while (r>=0&&r<8&&f>=0&&f<8) {
        const to = r*8+f;
        if (isFriend(board[to], turn)) break;
        moves.push(to);
        if (board[to]) break;
        r+=dr; f+=df;
      }
    }
  };

  if (type === 'r') slide([[1,0],[-1,0],[0,1],[0,-1]]);
  else if (type === 'b') slide([[1,1],[1,-1],[-1,1],[-1,-1]]);
  else if (type === 'q') slide([[1,0],[-1,0],[0,1],[0,-1],[1,1],[1,-1],[-1,1],[-1,-1]]);
  else if (type === 'n') {
    for (const [dr,df] of [[2,1],[2,-1],[-2,1],[-2,-1],[1,2],[1,-2],[-1,2],[-1,-2]]) {
      const r=rank+dr, f2=file+df;
      if (r>=0&&r<8&&f2>=0&&f2<8) addIfValid(r*8+f2);
    }
  }
  else if (type === 'k') {
    for (const [dr,df] of [[1,0],[-1,0],[0,1],[0,-1],[1,1],[1,-1],[-1,1],[-1,-1]]) {
      const r=rank+dr, f2=file+df;
      if (r>=0&&r<8&&f2>=0&&f2<8) addIfValid(r*8+f2);
    }
    // Castling
    const backRank = turn === 'w' ? 0 : 7;
    if (rank === backRank && file === 4) {
      // Kingside
      if (castling.includes(turn==='w'?'K':'k') &&
          !board[backRank*8+5] && !board[backRank*8+6] &&
          board[backRank*8+7] === (turn==='w'?'R':'r')) {
        moves.push(from + 2); // flagged later
      }
      // Queenside
      if (castling.includes(turn==='w'?'Q':'q') &&
          !board[backRank*8+3] && !board[backRank*8+2] && !board[backRank*8+1] &&
          board[backRank*8+0] === (turn==='w'?'R':'r')) {
        moves.push(from - 2);
      }
    }
  }
  else if (type === 'p') {
    const dir = turn === 'w' ? 1 : -1;
    const startRank = turn === 'w' ? 1 : 6;
    // Forward
    const fwd = from + dir*8;
    if (fwd>=0&&fwd<64&&!board[fwd]) {
      moves.push(fwd);
      const fwd2 = from + dir*16;
      if (rank===startRank && !board[fwd2]) moves.push(fwd2);
    }
    // Captures
    for (const df of [-1,1]) {
      const f2 = file+df;
      if (f2<0||f2>7) continue;
      const to = (rank+dir)*8+f2;
      if (to>=0&&to<64) {
        if (isEnemy(board[to], turn)) moves.push(to);
        else if (enPassant !== null && to === enPassant) moves.push(to);
      }
    }
  }
  return moves;
}

function isSquareAttacked(board, sq, byColor) {
  const tempTurn = byColor;
  // Check if any piece of byColor attacks sq
  for (let i=0;i<64;i++) {
    if (!board[i]||pieceColor(board[i])!==byColor) continue;
    const moves = pseudoMoves(board, i, byColor, null, '');
    if (moves.includes(sq)) return true;
  }
  return false;
}

function findKing(board, color) {
  const k = color === 'w' ? 'K' : 'k';
  for (let i=0;i<64;i++) if (board[i]===k) return i;
  return -1;
}

function isInCheck(board, color) {
  const kSq = findKing(board, color);
  if (kSq < 0) return false;
  return isSquareAttacked(board, kSq, color==='w'?'b':'w');
}

function applyMove(board, from, to, promotion, enPassant, castling) {
  const b = board.slice();
  const p = b[from];
  const turn = pieceColor(p);
  const type = pieceType(p);
  const rank = Math.floor(from/8), file = from%8;
  const toRank = Math.floor(to/8), toFile = to%8;
  let newEP = null;
  let newCastling = castling;

  // En passant capture
  if (type==='p' && enPassant !== null && to === enPassant) {
    const dir = turn==='w'?1:-1;
    b[to - dir*8] = null; // remove captured pawn
  }

  // Pawn double push — set EP square
  if (type==='p' && Math.abs(toRank-rank)===2) {
    const dir = turn==='w'?1:-1;
    newEP = from + dir*8;
  }

  // Castling move
  if (type==='k' && Math.abs(toFile-file)===2) {
    const backRank = turn==='w'?0:7;
    if (toFile===6) { // kingside
      b[backRank*8+5] = b[backRank*8+7];
      b[backRank*8+7] = null;
    } else { // queenside
      b[backRank*8+3] = b[backRank*8+0];
      b[backRank*8+0] = null;
    }
  }

  // Update castling rights
  if (type==='k') newCastling = newCastling.replace(turn==='w'?'K':'k','').replace(turn==='w'?'Q':'q','');
  if (type==='r') {
    if (from === 0) newCastling = newCastling.replace('Q','');
    if (from === 7) newCastling = newCastling.replace('K','');
    if (from === 56) newCastling = newCastling.replace('q','');
    if (from === 63) newCastling = newCastling.replace('k','');
  }
  // Rook captured
  if (to===0) newCastling=newCastling.replace('Q','');
  if (to===7) newCastling=newCastling.replace('K','');
  if (to===56) newCastling=newCastling.replace('q','');
  if (to===63) newCastling=newCastling.replace('k','');

  b[to] = promotion ? (turn==='w'?promotion.toUpperCase():promotion.toLowerCase()) : p;
  b[from] = null;

  return { board: b, newEP, newCastling };
}

function legalMoves(board, turn, enPassant, castling) {
  const all = [];
  for (let from=0;from<64;from++) {
    if (!board[from]||pieceColor(board[from])!==turn) continue;
    const pMoves = pseudoMoves(board, from, turn, enPassant, castling);
    for (const to of pMoves) {
      const type = pieceType(board[from]);
      const toRank = Math.floor(to/8);
      const file = from%8; const toFile = to%8;

      // Castling legality check: squares must not be attacked
      if (type==='k'&&Math.abs(toFile-file)===2) {
        const backRank = turn==='w'?0:7;
        const kingPath = toFile===6
          ? [backRank*8+4, backRank*8+5, backRank*8+6]
          : [backRank*8+4, backRank*8+3, backRank*8+2];
        const enemy = turn==='w'?'b':'w';
        if (kingPath.some(sq=>isSquareAttacked(board,sq,enemy))) continue;
      }

      const { board: nb } = applyMove(board, from, to, null, enPassant, castling);
      if (!isInCheck(nb, turn)) {
        // Promotion — expand
        if (type==='p' && (toRank===0||toRank===7)) {
          for (const promo of ['q','r','b','n']) all.push({from,to,promo});
        } else {
          all.push({from,to,promo:null});
        }
      }
    }
  }
  return all;
}

function gamePhase(board) {
  let p = 0;
  for (let i = 0; i < 64; i++) {
    const pc = board[i];
    if (pc) { const w = PHASE_WEIGHTS[pieceType(pc)]; if (w) p += w; }
  }
  return Math.min(p, MAX_PHASE);
}

function taper(mg, eg, phase) {
  return Math.round((mg * phase + eg * (MAX_PHASE - phase)) / MAX_PHASE);
}

function mirrorSq(sq) { return (7 - Math.floor(sq / 8)) * 8 + (sq % 8); }

function getPST(type, isWhite, sq, phase) {
  const idx = isWhite ? sq : mirrorSq(sq);
  switch (type) {
    case 'p': return taper(PAWN_MG_PST[idx],   PAWN_EG_PST[idx],   phase);
    case 'n': return taper(KNIGHT_MG_PST[idx],  KNIGHT_EG_PST[idx], phase);
    case 'b': return BISHOP_PST[idx];
    case 'r': return taper(ROOK_MG_PST[idx],    ROOK_EG_PST[idx],   phase);
    case 'q': return QUEEN_PST[idx];
    case 'k': return taper(KING_MG_PST[idx],    KING_EG_PST[idx],   phase);
    default:  return 0;
  }
}

function evalRooksMGEG(board, wPawnFiles, bPawnFiles, out) {
  for (let i = 0; i < 64; i++) {
    const p = board[i]; if (!p || pieceType(p) !== 'r') continue;
    const f = i % 8;
    const isWhite = pieceColor(p) === 'w';
    const noFriend = isWhite ? !wPawnFiles.has(f) : !bPawnFiles.has(f);
    const noEnemy  = isWhite ? !bPawnFiles.has(f) : !wPawnFiles.has(f);
    if (noFriend) {
      const bMG = noEnemy ? 25 : 10;
      const bEG = noEnemy ? 18 :  5;
      if (isWhite) { out.mg += bMG; out.eg += bEG; }
      else         { out.mg -= bMG; out.eg -= bEG; }
    }
    // Rook on 7th rank (relative) — Stockfish ~16 MG / +30 EG when enemy king is on
    // back rank or enemy has pawns there.
    const r = Math.floor(i / 8);
    const seventh = isWhite ? 6 : 1;
    if (r === seventh) {
      if (isWhite) { out.mg += 16; out.eg += 30; }
      else         { out.mg -= 16; out.eg -= 30; }
    }
  }
}

function evalKnightOutpostsMGEG(board, wPawns, bPawns, out) {
  for (let i = 0; i < 64; i++) {
    const p = board[i]; if (!p || pieceType(p) !== 'n') continue;
    const f = i % 8, r = Math.floor(i / 8);
    const isWhite = pieceColor(p) === 'w';
    if (isWhite) {
      if (r < 3) continue; // must be at least the 4th rank
      // Enemy pawns AHEAD of the knight = higher rank (toward black).
      const safe = !bPawns.some(sq => Math.abs((sq % 8) - f) === 1 && Math.floor(sq / 8) > r);
      if (safe) {
        // Stronger outposts deeper in enemy territory
        const baseMG = (r >= 4) ? 22 : 10;
        const baseEG = (r >= 4) ? 10 :  5;
        out.mg += baseMG; out.eg += baseEG;
        // Supported by friendly pawn behind on adjacent file (white pawn on r-1)
        if (wPawns.some(sq => Math.abs((sq % 8) - f) === 1 && Math.floor(sq / 8) === r - 1)) {
          out.mg += 12; out.eg += 5;
        }
      }
    } else {
      if (r > 4) continue;
      // Enemy pawns AHEAD of the black knight = lower rank (toward white).
      const safe = !wPawns.some(sq => Math.abs((sq % 8) - f) === 1 && Math.floor(sq / 8) < r);
      if (safe) {
        const baseMG = (r <= 3) ? 22 : 10;
        const baseEG = (r <= 3) ? 10 :  5;
        out.mg -= baseMG; out.eg -= baseEG;
        if (bPawns.some(sq => Math.abs((sq % 8) - f) === 1 && Math.floor(sq / 8) === r + 1)) {
          out.mg -= 12; out.eg -= 5;
        }
      }
    }
  }
}

function evalThreatsMGEG(board, out) {
  const wpAttack = new Array(64).fill(false), bpAttack = new Array(64).fill(false);
  for (let i = 0; i < 64; i++) {
    const p = board[i]; if (!p || pieceType(p) !== 'p') continue;
    const r = Math.floor(i / 8), f = i % 8;
    if (pieceColor(p) === 'w') {
      if (r + 1 < 8 && f - 1 >= 0) wpAttack[(r+1)*8 + (f-1)] = true;
      if (r + 1 < 8 && f + 1 < 8)  wpAttack[(r+1)*8 + (f+1)] = true;
    } else {
      if (r - 1 >= 0 && f - 1 >= 0) bpAttack[(r-1)*8 + (f-1)] = true;
      if (r - 1 >= 0 && f + 1 < 8)  bpAttack[(r-1)*8 + (f+1)] = true;
    }
  }
  for (let i = 0; i < 64; i++) {
    const p = board[i]; if (!p) continue;
    const t = pieceType(p);
    if (t === 'p' || t === 'k') continue;
    if (pieceColor(p) === 'b' && wpAttack[i]) {
      out.mg += THREAT_BY_PAWN_MG[t] || 0;
      out.eg += THREAT_BY_PAWN_EG[t] || 0;
    } else if (pieceColor(p) === 'w' && bpAttack[i]) {
      out.mg -= THREAT_BY_PAWN_MG[t] || 0;
      out.eg -= THREAT_BY_PAWN_EG[t] || 0;
    }
  }
}

function evalSpaceMG(board, wPawns, bPawns) {
  // Compute enemy-pawn-attacked squares.
  const wpAtk = new Array(64).fill(false), bpAtk = new Array(64).fill(false);
  for (const sq of wPawns) {
    const r = Math.floor(sq/8), f = sq%8;
    if (r+1<8 && f-1>=0) wpAtk[(r+1)*8+(f-1)] = true;
    if (r+1<8 && f+1<8 ) wpAtk[(r+1)*8+(f+1)] = true;
  }
  for (const sq of bPawns) {
    const r = Math.floor(sq/8), f = sq%8;
    if (r-1>=0 && f-1>=0) bpAtk[(r-1)*8+(f-1)] = true;
    if (r-1>=0 && f+1<8 ) bpAtk[(r-1)*8+(f+1)] = true;
  }
  // For each side, count safe central-zone squares.  Central files c-f, ranks 2-4
  // for white, ranks 3-5 for black (mirrored).
  let wSpace = 0, bSpace = 0;
  for (let f = 2; f <= 5; f++) {
    for (let r = 1; r <= 3; r++) {
      const sq = r*8 + f;
      if (!bpAtk[sq] && board[sq] === null) wSpace++;
    }
    for (let r = 4; r <= 6; r++) {
      const sq = r*8 + f;
      if (!wpAtk[sq] && board[sq] === null) bSpace++;
    }
  }
  // Space matters more when we still have minor pieces behind it (heavier
  // pieces benefit from open ranks).  Weight by friendly minor count.
  let wMinors = 0, bMinors = 0;
  for (let i = 0; i < 64; i++) {
    const p = board[i]; if (!p) continue;
    const t = pieceType(p);
    if (t !== 'n' && t !== 'b') continue;
    if (pieceColor(p) === 'w') wMinors++; else bMinors++;
  }
  return (wSpace * (wMinors + 1) - bSpace * (bMinors + 1)) * 2;
}

function kingZoneSafety(board, color) {
  const kSq = findKing(board, color);
  if (kSq < 0) return { mg: 0, eg: 0 };
  const kR = Math.floor(kSq/8), kF = kSq%8;
  const dir = color === 'w' ? 1 : -1;
  const pawn = color === 'w' ? 'P' : 'p';
  const enemy = color === 'w' ? 'b' : 'w';

  // Pawn shelter (MG-only — endgame king-safety has no walls).
  let shelter = 0;
  for (let df = -1; df <= 1; df++) {
    const sf = kF + df; if (sf < 0 || sf > 7) continue;
    const s1 = (kR + dir) * 8 + sf, s2 = (kR + 2*dir) * 8 + sf;
    if      (s1 >= 0 && s1 < 64 && board[s1] === pawn) shelter += 14;
    else if (s2 >= 0 && s2 < 64 && board[s2] === pawn) shelter += 7;
    else                                                shelter -= 16;
  }
  // Open-file penalty near the king (MG-only).
  for (let df = -1; df <= 1; df++) {
    const f = kF + df; if (f < 0 || f > 7) continue;
    let has = false;
    for (let r = 0; r < 8; r++) if (board[r*8+f] === pawn) { has = true; break; }
    if (!has) shelter -= 18;
  }

  // King-zone = the 3x3 area around the king (plus the three squares directly
  // in front).  Count enemy attackers and total attack units.
  const zone = new Set();
  for (let dr = -1; dr <= 1; dr++)
    for (let df = -1; df <= 1; df++) {
      const nr = kR + dr, nf = kF + df;
      if (nr >= 0 && nr < 8 && nf >= 0 && nf < 8) zone.add(nr*8 + nf);
    }
  // Extend zone two squares in front of the king (front of pawn shield).
  for (let df = -1; df <= 1; df++) {
    const nf = kF + df; if (nf < 0 || nf > 7) continue;
    const nr = kR + 2*dir;
    if (nr >= 0 && nr < 8) zone.add(nr*8 + nf);
  }
  let attackers = 0, attackUnits = 0;
  for (let j = 0; j < 64; j++) {
    const ap = board[j]; if (!ap || pieceColor(ap) !== enemy) continue;
    const at = pieceType(ap);
    if (at === 'p' || at === 'k') continue;
    const moves = pseudoMoves(board, j, enemy, null, '');
    let attacksZone = false;
    for (const m of moves) if (zone.has(m)) { attacksZone = true; break; }
    if (attacksZone) {
      attackers++;
      attackUnits += ATTACK_UNIT[at] || 0;
    }
  }
  // Stockfish multiplies attack-unit pressure non-linearly with the number of
  // distinct attackers.  Approximate by an additional factor of (attackers-1)/4.
  let attack = 0;
  if (attackers >= 2) {
    attack = Math.round(attackUnits * (1 + (attackers - 2) * 0.3) / 4);
  }
  // shelter & attack are penalties to *this color's* king, so we return them
  // as a contribution to that side (positive = safer king = good).
  return { mg: shelter - attack, eg: 0 };
}

function phase128(board) {
  return Math.round(gamePhase(board) * 128 / MAX_PHASE);
}

function scaleFactor(board, egScoreWhitePOV) {
  let wB = 0, bB = 0, wBcolor = -1, bBcolor = -1;
  let wMinors = 0, bMinors = 0, wRooks = 0, bRooks = 0, wQ = 0, bQ = 0;
  let wPawns = 0, bPawns = 0;
  for (let i = 0; i < 64; i++) {
    const p = board[i]; if (!p) continue;
    const t = pieceType(p), c = pieceColor(p);
    if (c === 'w') {
      if (t === 'p') wPawns++;
      else if (t === 'n' || t === 'b') wMinors++;
      else if (t === 'r') wRooks++;
      else if (t === 'q') wQ++;
      if (t === 'b') { wB++; wBcolor = ((Math.floor(i/8) + (i%8)) & 1); }
    } else {
      if (t === 'p') bPawns++;
      else if (t === 'n' || t === 'b') bMinors++;
      else if (t === 'r') bRooks++;
      else if (t === 'q') bQ++;
      if (t === 'b') { bB++; bBcolor = ((Math.floor(i/8) + (i%8)) & 1); }
    }
  }
  // Opposite-coloured bishops with no other pieces: notoriously drawish.
  if (wB === 1 && bB === 1 && wBcolor !== bBcolor &&
      wMinors === 1 && bMinors === 1 && wRooks === 0 && bRooks === 0 && wQ === 0 && bQ === 0) {
    // Scale based on pawn count — more pawns = slightly more winning chances.
    return Math.min(36, 16 + Math.abs(wPawns - bPawns) * 4);
  }
  // Side with material advantage but no pawns: very hard to win.
  const winnerIsWhite = egScoreWhitePOV > 0;
  const winnerPawns = winnerIsWhite ? wPawns : bPawns;
  if (winnerPawns === 0) {
    const adv = Math.abs(egScoreWhitePOV);
    if (adv < 400) return 8;     // K+minor vs K: dead draw
    if (adv < 700) return 24;    // small advantage, scaled down
  }
  return 64;
}

function pawnStructureMGEG(wP, bP) {
  let mg = 0, eg = 0;
  const wF = new Array(8).fill(0), bF = new Array(8).fill(0);
  for (const sq of wP) wF[sq % 8]++;
  for (const sq of bP) bF[sq % 8]++;

  // Doubled pawns
  for (let f = 0; f < 8; f++) {
    if (wF[f] > 1) { mg -= 11 * (wF[f] - 1); eg -= 56 * (wF[f] - 1); }
    if (bF[f] > 1) { mg += 11 * (bF[f] - 1); eg += 56 * (bF[f] - 1); }
    const wN = (f > 0 ? wF[f-1] : 0) + (f < 7 ? wF[f+1] : 0);
    const bN = (f > 0 ? bF[f-1] : 0) + (f < 7 ? bF[f+1] : 0);
    if (wF[f] > 0 && wN === 0) { mg -= 5;  eg -= 15; }
    if (bF[f] > 0 && bN === 0) { mg += 5;  eg += 15; }
  }
  // Passed pawns (rank-scaled)
  for (const sq of wP) {
    const f = sq % 8, r = Math.floor(sq / 8);
    let pass = true;
    for (const b of bP) if (Math.abs((b%8)-f)<=1 && Math.floor(b/8)>r) { pass=false; break; }
    if (pass) { mg += PASSED_MG[r]; eg += PASSED_EG[r]; }
  }
  for (const sq of bP) {
    const f = sq % 8, r = Math.floor(sq / 8);
    let pass = true;
    for (const w of wP) if (Math.abs((w%8)-f)<=1 && Math.floor(w/8)<r) { pass=false; break; }
    if (pass) { mg -= PASSED_MG[7-r]; eg -= PASSED_EG[7-r]; }
  }
  // Connected pawns (small structural bonus, MG-heavy)
  const connected = (pawns) => {
    let n = 0;
    for (const sq of pawns)
      for (const sq2 of pawns)
        if (sq2 !== sq && Math.abs((sq2%8)-(sq%8)) === 1 &&
            Math.abs(Math.floor(sq2/8)-Math.floor(sq/8)) <= 1) { n++; break; }
    return n;
  };
  const wc = connected(wP), bc = connected(bP);
  mg += (wc - bc) * 8;
  eg += (wc - bc) * 3;
  return { mg, eg };
}

function mobilityMGEG(board) {
  let wMG = 0, wEG = 0, bMG = 0, bEG = 0;
  const W = { n: 4, b: 5, r: 2, q: 1 };
  const E = { n: 3, b: 5, r: 4, q: 2 };
  for (let i = 0; i < 64; i++) {
    const p = board[i]; if (!p) continue;
    const t = pieceType(p);
    if (t === 'p' || t === 'k') continue;
    const c = pieceColor(p);
    const cnt = pseudoMoves(board, i, c, null, '').length;
    const mg = (W[t] || 0) * cnt;
    const eg = (E[t] || 0) * cnt;
    if (c === 'w') { wMG += mg; wEG += eg; }
    else           { bMG += mg; bEG += eg; }
  }
  return { mg: wMG - bMG, eg: wEG - bEG };
}

function evaluateMGEG(board) {
  const phase = gamePhase(board);
  let mg = 0, eg = 0;
  const wPawns = [], bPawns = [];
  let wB = 0, bB = 0;

  // Material + PSTs in one sweep.  PSTs already taper internally via getPST,
  // but for the proper main_evaluation pattern we need separate MG/EG totals,
  // so read the PSTs at the two phase extremes explicitly.
  for (let i = 0; i < 64; i++) {
    const p = board[i]; if (!p) continue;
    const t = pieceType(p);
    const white = pieceColor(p) === 'w';
    const pstMG = getPST(t, white, i, MAX_PHASE); // phase=MAX → pure MG
    const pstEG = getPST(t, white, i, 0);          // phase=0 → pure EG
    if (white) {
      mg += PV_MG[t] + pstMG;
      eg += PV_EG[t] + pstEG;
      if (t === 'p') wPawns.push(i);
      if (t === 'b') wB++;
    } else {
      mg -= PV_MG[t] + pstMG;
      eg -= PV_EG[t] + pstEG;
      if (t === 'p') bPawns.push(i);
      if (t === 'b') bB++;
    }
  }

  // Bishop pair
  if (wB >= 2) { mg += 22; eg += 30; }
  if (bB >= 2) { mg -= 22; eg -= 30; }

  // Pawn structure (we need MG/EG separation explicitly here)
  const ps = pawnStructureMGEG(wPawns, bPawns);
  mg += ps.mg; eg += ps.eg;

  // Mobility (separated MG/EG: extra moves matter more in MG)
  const mob = mobilityMGEG(board);
  mg += mob.mg; eg += mob.eg;

  // Rooks
  const rookOut = { mg: 0, eg: 0 };
  const wPawnFiles = new Set(wPawns.map(sq => sq % 8));
  const bPawnFiles = new Set(bPawns.map(sq => sq % 8));
  evalRooksMGEG(board, wPawnFiles, bPawnFiles, rookOut);
  mg += rookOut.mg; eg += rookOut.eg;

  // Knight outposts
  const opOut = { mg: 0, eg: 0 };
  evalKnightOutpostsMGEG(board, wPawns, bPawns, opOut);
  mg += opOut.mg; eg += opOut.eg;

  // Threats (MG-heavy)
  const thr = { mg: 0, eg: 0 };
  evalThreatsMGEG(board, thr);
  mg += thr.mg; eg += thr.eg;

  // Space (MG-only — useless in endgame)
  mg += evalSpaceMG(board, wPawns, bPawns);

  // King safety (mg + small eg portion).  Already returns white-pov when we
  // diff (w - b).  Scale by phase fraction inside the function callers so we
  // don't double-taper.
  const wKing = kingZoneSafety(board, 'w');
  const bKing = kingZoneSafety(board, 'b');
  mg += wKing.mg - bKing.mg;
  eg += (wKing.eg - bKing.eg) >> 1; // king-safety contribution fades in EG

  return { mg, eg };
}

function evaluateBoard(board, turn) {
  const { mg, eg } = evaluateMGEG(board);
  const p   = phase128(board);
  const sf  = scaleFactor(board, eg);
  // Tapered with scale factor applied to EG side (Stockfish: scaled-eg / 64).
  const scaledEG = Math.trunc(eg * sf / 64);
  let v = Math.trunc((mg * p + scaledEG * (128 - p)) / 128);
  // Tempo: side-to-move bonus, in white POV terms.
  if (turn) v += turn === 'w' ? TEMPO_BONUS : -TEMPO_BONUS;
  return v;
}

function positionHash(board, turn, castling, ep) {
  let h = 0x811c9dc5 | 0; // FNV offset basis
  for (let i = 0; i < 64; i++) {
    const c = board[i] ? board[i].charCodeAt(0) : 32;
    h = (h ^ c) >>> 0;
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  h = (h ^ (turn === 'w' ? 87 : 66)) >>> 0;
  h = Math.imul(h, 0x01000193) >>> 0;
  for (let i = 0; i < castling.length; i++) {
    h = (h ^ castling.charCodeAt(i)) >>> 0;
    h = Math.imul(h, 0x01000193) >>> 0;
  }
  h = (h ^ ((ep == null) ? 255 : ep)) >>> 0;
  h = Math.imul(h, 0x01000193) >>> 0;
  return h >>> 0;
}

function historyIdx(piece, to) { return _pieceIdx[piece] * 64 + to; }

function moveOrderScoreEx(board, mv, ep, ttMove, ply) {
  const { from, to, promo } = mv;
  const piece = board[from]; if (!piece) return 0;
  const target = board[to];
  const t = pieceType(piece);
  const white = pieceColor(piece) === 'w';

  if (ttMove && from === ttMove.from && to === ttMove.to && promo === ttMove.promo)
    return 1e9; // PV move from previous iteration

  let s = 0;
  if (target) {
    s += 100000 + 10 * PIECE_VALUES[pieceType(target)] - PIECE_VALUES[t];
  } else if (ep !== null && to === ep && t === 'p') {
    s += 100100;
  } else {
    // Quiet move — killer & history
    const killers = _killers[ply];
    if (killers && killers[0] && killers[0].from === from && killers[0].to === to && killers[0].promo === promo) s += 90000;
    else if (killers && killers[1] && killers[1].from === from && killers[1].to === to && killers[1].promo === promo) s += 80000;
    else s += _history[historyIdx(piece, to)];
  }
  if (promo) s += 95000 + PIECE_VALUES[promo];
  // Tie-break with PST delta so identically-scored moves still get sensible ordering.
  s += getPST(t, white, to, MAX_PHASE) - getPST(t, white, from, MAX_PHASE);
  return s;
}

function orderMovesEx(board, moves, ep, ttMove, ply) {
  return moves.map(m => [m, moveOrderScoreEx(board, m, ep, ttMove, ply)])
              .sort((a, b) => b[1] - a[1])
              .map(x => x[0]);
}

function quiesce(board, turn, ep, castling, alpha, beta, qdepth) {
  if ((++_nodeCount & 1023) === 0 && Date.now() > _searchDeadline) throw 'timeout';

  // White-POV eval → side-to-move POV
  const ev = evaluateBoard(board, turn);
  const standPat = turn === 'w' ? ev : -ev;
  if (standPat >= beta) return standPat;

  let best = standPat;
  if (standPat > alpha) alpha = standPat;
  if (qdepth <= 0) return best;

  const all = legalMoves(board, turn, ep, castling);
  const captures = all.filter(m =>
    board[m.to] !== null ||
    (ep !== null && m.to === ep && pieceType(board[m.from]) === 'p') ||
    m.promo
  );
  if (captures.length === 0) return best;

  // Simple "delta pruning": if even capturing the most valuable enemy piece
  // can't get within a queen of alpha, skip the q-search entirely.  Cheap
  // safety net against ridiculous capture chains. (~10 Elo.)
  const DELTA_MARGIN = 200;
  for (const mv of orderMovesEx(board, captures, ep, null, 0)) {
    const target = board[mv.to];
    const gain = (target ? PIECE_VALUES[pieceType(target)] : 0) +
                 (mv.promo ? PIECE_VALUES[mv.promo] - PIECE_VALUES.p : 0);
    if (standPat + gain + DELTA_MARGIN < alpha) continue;

    const { board: nb, newEP, newCastling } = applyMove(board, mv.from, mv.to, mv.promo, ep, castling);
    const score = -quiesce(nb, turn==='w'?'b':'w', newEP, newCastling||'', -beta, -alpha, qdepth-1);
    if (score > best) best = score;
    if (best >= beta) return best;
    if (best > alpha) alpha = best;
  }
  return best;
}

function negamax(board, turn, ep, castling, depth, alpha, beta, ply, allowNull) {
  if ((++_nodeCount & 1023) === 0 && Date.now() > _searchDeadline) throw 'timeout';

  const inCheck = isInCheck(board, turn);

  // Check extension — search +1 ply if we're being chased.  Must be done
  // BEFORE depth <= 0 short-circuits to quiesce.
  if (inCheck) depth += 1;

  // Mate-distance pruning: never report a mate worse than what we already have.
  const alphaMD = Math.max(alpha, -MATE_SCORE + ply);
  const betaMD  = Math.min(beta,   MATE_SCORE - ply);
  if (alphaMD >= betaMD) return alphaMD;
  alpha = alphaMD; beta = betaMD;

  if (depth <= 0) return quiesce(board, turn, ep, castling, alpha, beta, QMAX);

  // ---- Transposition-table probe ----
  const hash = positionHash(board, turn, castling, ep);
  const tt   = ttProbe(hash);
  let ttMove = tt ? tt.move : null;
  if (tt && tt.depth >= depth) {
    const v = tt.score;
    if (tt.bound === TT_EXACT) return v;
    if (tt.bound === TT_LOWER && v >= beta)  return v;
    if (tt.bound === TT_UPPER && v <= alpha) return v;
  }

  const moves = legalMoves(board, turn, ep, castling);
  if (moves.length === 0)
    return inCheck ? -(MATE_SCORE - ply) : 0; // checkmate at this ply / stalemate

  // ---- Compute static eval once for RFP + futility pruning (depth 1–4 only) ----
  // Gated on !inCheck so we don't prune out-of-check nodes incorrectly.
  let rfpStaticEval = null;
  if (!inCheck && depth <= 4) {
    const _ev = evaluateBoard(board, turn);
    rfpStaticEval = turn === 'w' ? _ev : -_ev;
  }

  // ---- Reverse Futility Pruning (static null move) ----
  // If our static eval is already well above beta by a depth-scaled margin,
  // this node will almost certainly fail high — prune it immediately.
  // Avoids wasting nodes on clearly-winning positions at shallow depth.
  if (rfpStaticEval !== null && Math.abs(beta) < MATE_SCORE - 100) {
    if (rfpStaticEval - 120 * depth >= beta) return rfpStaticEval;
  }

  // ---- Null-move pruning ----
  // Skip our turn; if a 2-ply-shallower search still fails high, the actual
  // position would have too.  Disabled in check, near leaves, and when our
  // side has only king+pawns (zugzwang risk).
  if (allowNull && !inCheck && depth >= 3 && hasNonPawnMaterial(board, turn)) {
    const R = depth >= 6 ? 3 : 2; // reduction
    const score = -negamax(board, turn==='w'?'b':'w', null, castling, depth - 1 - R,
                           -beta, -beta + 1, ply + 1, false);
    if (score >= beta) return score;
  }

  // ---- Move ordering ----
  const ordered = orderMovesEx(board, moves, ep, ttMove, ply);

  let best = -Infinity;
  let bestMove = ordered[0];
  let bound = TT_UPPER;       // assume fail-low until we raise alpha
  let moveIdx = 0;

  // Reuse static eval (computed above) for futility pruning at depth 1–2.
  // fpEval is -Infinity when not applicable (not in check is already required
  // by rfpStaticEval being non-null).
  const fpEval = (depth <= 2 && rfpStaticEval !== null) ? rfpStaticEval : -Infinity;

  for (const mv of ordered) {
    const isCapture = board[mv.to] !== null ||
                      (ep !== null && mv.to === ep && pieceType(board[mv.from]) === 'p');
    const isQuiet   = !isCapture && !mv.promo;

    // ---- Futility Pruning ----
    // At depth 1–2, skip quiet moves whose best-case gain can't reach alpha.
    // Only applied after the first (PV) move so we always search at least one.
    if (fpEval !== -Infinity && isQuiet && moveIdx > 0) {
      const fpMargin = depth === 1 ? 300 : 500;
      if (fpEval + fpMargin <= alpha) continue;
    }

    const { board: nb, newEP, newCastling } = applyMove(board, mv.from, mv.to, mv.promo, ep, castling);

    let score;
    if (moveIdx === 0) {
      // PV move: full window
      score = -negamax(nb, turn==='w'?'b':'w', newEP, newCastling||'', depth-1, -beta, -alpha, ply+1, true);
    } else {
      // ---- Late-move reductions for quiet moves deep in the ordering ----
      let reduction = 0;
      if (depth >= 3 && isQuiet && !inCheck && moveIdx >= 3) {
        reduction = 1 + Math.floor(Math.log2(depth) * Math.log2(moveIdx) / 2.5);
        if (reduction > depth - 2) reduction = depth - 2;
        if (reduction < 0) reduction = 0;
      }
      // Zero-window scout search at (possibly reduced) depth.
      score = -negamax(nb, turn==='w'?'b':'w', newEP, newCastling||'',
                        depth - 1 - reduction, -alpha - 1, -alpha, ply+1, true);
      // If the reduced/scout search beats alpha, re-search at full depth
      // with the full window.
      if (score > alpha && (reduction > 0 || score < beta)) {
        score = -negamax(nb, turn==='w'?'b':'w', newEP, newCastling||'', depth-1, -beta, -alpha, ply+1, true);
      }
    }

    if (score > best) { best = score; bestMove = mv; }
    if (best > alpha) { alpha = best; bound = TT_EXACT; }
    if (alpha >= beta) {
      bound = TT_LOWER;
      // Save killer + bump history for quiet beta-cutoff moves.
      if (isQuiet) {
        const kil = _killers[ply];
        if (!kil[0] || kil[0].from !== mv.from || kil[0].to !== mv.to) {
          kil[1] = kil[0];
          kil[0] = mv;
        }
        const pc = board[mv.from];
        if (pc) _history[historyIdx(pc, mv.to)] += depth * depth;
      }
      break;
    }
    moveIdx++;
  }

  ttStore(hash, depth, best, bound, bestMove);
  return best;
}

function hasNonPawnMaterial(board, turn) {
  const upper = turn === 'w';
  for (let i = 0; i < 64; i++) {
    const p = board[i]; if (!p) continue;
    if ((pieceColor(p) === 'w') !== upper) continue;
    const t = pieceType(p);
    if (t !== 'p' && t !== 'k') return true;
  }
  return false;
}

function computerMove(gameState) {
  const { board, turn, enPassant, castling } = gameState;
  const moves = legalMoves(board, turn, enPassant, castling);
  if (moves.length === 0) return null;
  if (moves.length === 1) {
    // Even forced moves should update the score so the bar reflects reality.
    lastSearchScore = evaluateBoard(board, turn);
    lastSearchDepth = 0;
    return moves[0];
  }

  // Depth and time limits can be overridden per-request (difficulty system).
  // Defaults keep the original Hard behaviour when no override is supplied.
  const MAX_DEPTH  = gameState.maxDepth !== undefined ? gameState.maxDepth  : 10;
  const TIME_LIMIT = gameState.searchMs !== undefined ? gameState.searchMs  : 2200;

  _searchDeadline = Date.now() + TIME_LIMIT;
  _nodeCount = 0;
  ttClear();
  clearHistory();

  let bestMove = orderMovesEx(board, moves, enPassant, null, 0)[0];
  let bestScoreSTM = -Infinity; // side-to-move POV; converted at end

  // Aspiration window from previous iteration's score.
  let windowSTM = 0; // start with no previous score → infinite window first iter
  let prevScoreSTM = 0;

  for (let depth = 1; depth <= MAX_DEPTH; depth++) {
    const ordered = orderMovesEx(board, moves, enPassant,
                                 bestMove ? { from: bestMove.from, to: bestMove.to, promo: bestMove.promo } : null,
                                 0);

    let iterBest  = ordered[0];
    let iterScore = -Infinity;
    let timedOut  = false;

    // Aspiration: start with a tight window around prevScore, widen on fail.
    let lo = depth === 1 ? -Infinity : prevScoreSTM - 50;
    let hi = depth === 1 ?  Infinity : prevScoreSTM + 50;
    let attempt = 0;

    // Repeat the iteration until we land inside the aspiration window.
    aspiration: while (true) {
      iterBest  = ordered[0];
      iterScore = -Infinity;
      let alpha = lo;
      let beta  = hi;
      let mvIdx = 0;
      try {
        for (const mv of ordered) {
          const { board: nb, newEP, newCastling } = applyMove(board, mv.from, mv.to, mv.promo, enPassant, castling);
          let score;
          if (mvIdx === 0) {
            score = -negamax(nb, turn==='w'?'b':'w', newEP, newCastling||'', depth-1, -beta, -alpha, 1, true);
          } else {
            // PVS at root: scout, then re-search if needed
            score = -negamax(nb, turn==='w'?'b':'w', newEP, newCastling||'', depth-1, -alpha-1, -alpha, 1, true);
            if (score > alpha && score < beta) {
              score = -negamax(nb, turn==='w'?'b':'w', newEP, newCastling||'', depth-1, -beta, -alpha, 1, true);
            }
          }
          if (score > iterScore) { iterScore = score; iterBest = mv; }
          if (iterScore > alpha) alpha = iterScore;
          mvIdx++;
        }
      } catch(e) {
        if (e === 'timeout') { timedOut = true; break aspiration; }
        throw e;
      }

      // Aspiration window check
      if (iterScore <= lo)      { lo = -Infinity; attempt++; if (attempt < 2) continue aspiration; }
      else if (iterScore >= hi) { hi =  Infinity; attempt++; if (attempt < 2) continue aspiration; }
      break;
    }

    if (!timedOut) {
      bestMove     = iterBest;
      bestScoreSTM = iterScore;
      prevScoreSTM = iterScore;
      lastSearchDepth = depth;
      // Place best move first so the next iteration prunes harder.
      const idx = moves.findIndex(m => m.from === iterBest.from && m.to === iterBest.to && m.promo === iterBest.promo);
      if (idx > 0) { moves.splice(idx, 1); moves.unshift(iterBest); }
    }

    if (timedOut || Date.now() > _searchDeadline) break;
  }

  // Convert side-to-move score → white's POV for the eval bar.
  lastSearchScore = turn === 'w' ? bestScoreSTM : -bestScoreSTM;

  return bestMove;
}

function searchEval(board, turn, ep, castling) {
  const moves = legalMoves(board, turn, ep, castling);
  if (moves.length === 0) {
    if (isInCheck(board, turn)) return turn === 'w' ? -MATE_SCORE : MATE_SCORE;
    return 0;
  }
  _searchDeadline = Date.now() + 1500;
  _nodeCount = 0;
  ttClear();
  clearHistory();

  let bestSTM = 0;
  try {
    // Iterative deepening for analysis too — gives a sharper, more stable bar.
    for (let d = 1; d <= 10; d++) {
      bestSTM = negamax(board, turn, ep, castling, d, -Infinity, Infinity, 0, true);
      if (Date.now() > _searchDeadline) break;
    }
  } catch(e) {
    if (e !== 'timeout') throw e;
    // Fall back to whatever we've already computed.
  }
  return turn === 'w' ? bestSTM : -bestSTM;
}

function parseFen(fen) {
  const parts = fen.split(' ');
  const board = new Array(64).fill(null);
  let rank = 7, file = 0;
  for (const ch of parts[0]) {
    if (ch === '/') { rank--; file = 0; }
    else if (ch >= '1' && ch <= '8') { file += +ch; }
    else { board[rank*8+file] = ch; file++; }
  }
  return {
    board,
    turn: parts[1] === 'w' ? 'w' : 'b',
    castling: parts[2],
    enPassant: parts[3] === '-' ? null : algebraicToIndex(parts[3]),
    halfMove: +parts[4],
    fullMove: +parts[5],
  };
}

function algebraicToIndex(alg) {
  const file = alg.charCodeAt(0) - 97;
  const rank = +alg[1] - 1;
  return rank*8+file;
}

function indexToAlgebraic(idx) {
  const file = idx % 8;
  const rank = Math.floor(idx / 8);
  return String.fromCharCode(97+file) + (rank+1);
}

function moveToNotation(board, from, to, promo, enPassant, castling) {
  const p = board[from];
  const type = pieceType(p);
  const file = from%8; const toFile = to%8;

  if (type==='k'&&Math.abs(toFile-file)===2) {
    return toFile===6 ? 'O-O' : 'O-O-O';
  }
  let n = '';
  if (type!=='p') n += type.toUpperCase();
  if (board[to]||(type==='p'&&to===enPassant)) {
    if (type==='p') n += String.fromCharCode(97+file);
    n += 'x';
  }
  n += indexToAlgebraic(to);
  if (promo) n += '='+promo.toUpperCase();
  return n;
}

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
