// Smoke test for the WebAssembly build: node web/scripts/smoke.mjs (after building the wasm
// preset). Asks every difficulty for a move from two positions, checks each move is legal, and
// prints the search stats. Exits 1 on any failure.

import createModule from '../../build/wasm/c4.js';

const DIFFICULTIES = ['easy', 'medium', 'hard', 'impossible'];
const POSITIONS = ['', '4453'];

const c4 = await createModule();
const chooseMove = c4.cwrap('c4_choose_move', 'number', ['string', 'number', 'number']);
const winningCells = c4.cwrap('c4_winning_cells', 'number', ['string']);
const stat = (name) => c4.ccall(name, 'number', [], []);

let failures = 0;
function check(ok, message) {
  if (!ok) {
    console.log(`FAIL: ${message}`);
    failures++;
  }
}

// Legal means a column 0 to 6 with fewer than 6 stones in it.
function isLegal(moves, col) {
  const height = [...moves].filter((m) => Number(m) - 1 === col).length;
  return Number.isInteger(col) && col >= 0 && col <= 6 && height < 6;
}

for (const moves of POSITIONS) {
  for (const [difficulty, name] of DIFFICULTIES.entries()) {
    const col = chooseMove(moves, difficulty, difficulty + 1);
    check(isLegal(moves, col), `${name} from "${moves}" returned illegal column ${col}`);
    console.log(
      `"${moves}" ${name.padEnd(10)} column ${col + 1}  depth ${stat('c4_last_depth')}` +
        `  score ${stat('c4_last_score')}  nodes ${stat('c4_last_nodes')}` +
        `  ${stat('c4_last_time_ms').toFixed(1)} ms` +
        `  ${(stat('c4_last_nps') / 1e6).toFixed(2)} M nodes/s`,
    );
  }
}

// X wins with four in column 1: bits 0 to 3.
check(winningCells('1212121') === 15, `winning cells of "1212121" = ${winningCells('1212121')}`);
check(winningCells('4453') === 0, 'no winning cells in an unfinished game');
check(chooseMove('1212121', 3, 1) === -1, 'no move after the game is won');

// The exported score scale (score.hpp: WIN_SCORE 10000, MIN_WIN_SCORE = WIN_SCORE - 42), and a
// real mate score on it: X to move in "121212" wins at once, at ply 1.
const winScore = stat('c4_win_score');
const minWinScore = stat('c4_min_win_score');
check(winScore === 10000 && minWinScore === 9958, `score scale ${winScore} / ${minWinScore}`);
check(chooseMove('121212', 2, 1) === 0, 'Hard plays the win in one');
check(stat('c4_last_score') === winScore - 1, `win in one scores ${stat('c4_last_score')}`);

console.log(failures === 0 ? 'Smoke test passed.' : `Smoke test failed: ${failures} check(s).`);
process.exit(failures === 0 ? 0 : 1);
