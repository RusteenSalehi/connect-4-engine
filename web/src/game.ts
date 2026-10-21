// The game as plain data: a list of columns in play order. The board, whose turn it is and which
// columns are open are all derived from that list, so undo and new game are just a shorter list.
// Wins are not detected here; the engine reports them (c4_winning_cells), so there is one
// definition of four in a row.

export const COLS = 7;
export const ROWS = 6;

export type Player = 1 | 2; // 1 moves first
export type Cell = 0 | Player; // 0 is empty
export type Moves = number[]; // columns 0 to 6

// The engine's move string uses digits 1 to 7.
export function toMoveString(moves: Moves): string {
  return moves.map((col) => col + 1).join('');
}

export function playerToMove(moves: Moves): Player {
  return moves.length % 2 === 0 ? 1 : 2;
}

export function height(moves: Moves, col: number): number {
  return moves.filter((c) => c === col).length;
}

export function canPlay(moves: Moves, col: number): boolean {
  return col >= 0 && col < COLS && height(moves, col) < ROWS;
}

export function isBoardFull(moves: Moves): boolean {
  return moves.length === COLS * ROWS;
}

// grid[col][row], row 0 at the bottom (the engine's layout too).
export function grid(moves: Moves): Cell[][] {
  const cells: Cell[][] = Array.from({ length: COLS }, () => Array<Cell>(ROWS).fill(0));
  moves.forEach((col, i) => {
    cells[col][height(moves.slice(0, i), col)] = i % 2 === 0 ? 1 : 2;
  });
  return cells;
}

// The engine's cell masks use bit col * 7 + row. The mask can use up to 48 bits, more than
// JavaScript's 32-bit bitwise operators handle, so the bit is read with plain arithmetic.
export function inMask(mask: number, col: number, row: number): boolean {
  return Math.floor(mask / 2 ** (col * 7 + row)) % 2 === 1;
}

// Player 1 makes the first move and player 2 the second, so the human has moved at least once
// exactly when there are at least `human` moves.
export function canUndo(moves: Moves, human: Player): boolean {
  return moves.length >= human;
}

// Takes back moves up to and including the human's last one (normally the engine's reply and
// the human move before it), so it is the human's turn again.
export function undo(moves: Moves, human: Player): Moves {
  const result = moves.slice();
  while (result.length > 0) {
    const mover = playerToMove(result) === 1 ? 2 : 1; // who made the last move
    result.pop();
    if (mover === human) break;
  }
  return result;
}
