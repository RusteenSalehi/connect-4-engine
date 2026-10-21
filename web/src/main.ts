// The page: builds the board once, keeps the game state, and re-renders from that state after
// every change. All engine work goes through engine.ts.

import './style.css';
import { chooseMove, DIFFICULTIES, type EngineMove, winningCells } from './engine';
import {
  canPlay,
  canUndo,
  COLS,
  grid,
  height,
  inMask,
  isBoardFull,
  type Moves,
  type Player,
  playerToMove,
  ROWS,
  toMoveString,
  undo,
} from './game';

// Instant Easy replies would land while the human's disc is still falling. This pause is only
// for display; the stats panel shows the engine's own measured time.
const MIN_THINKING_MS = 300;

// The engine's score scale, copied from engine/include/c4/score.hpp (keep in sync). A win at
// ply p from the search root scores WIN_SCORE - p; anything at least MIN_WIN_SCORE in size is
// a proven result rather than a heuristic.
const WIN_SCORE = 10000;
const MAX_MOVES = 42;
const MIN_WIN_SCORE = WIN_SCORE - MAX_MOVES;

// ---------------------------------------------------------------------------------------------
// State
// ---------------------------------------------------------------------------------------------

let moves: Moves = [];
let human: Player = 1;
let winMask = 0; // cells of the completed line(s); 0 while nobody has won
let busy = false; // waiting on the engine; all game input is ignored meanwhile
let thinking = false; // the engine is choosing a move (busy, plus the visible indicator)
let lastMove: EngineMove | null = null;
let error = '';

function gameOver(): boolean {
  return winMask !== 0 || isBoardFull(moves);
}

function humanCanMove(): boolean {
  return !busy && !error && !gameOver() && playerToMove(moves) === human;
}

// ---------------------------------------------------------------------------------------------
// Page elements
// ---------------------------------------------------------------------------------------------

function byId<T extends HTMLElement>(id: string): T {
  return document.getElementById(id) as T;
}

const boardEl = byId<HTMLDivElement>('board');
const statusEl = byId<HTMLParagraphElement>('status');
const spinnerEl = byId<HTMLSpanElement>('spinner');
const difficultyEl = byId<HTMLSelectElement>('difficulty');
const firstEl = byId<HTMLSelectElement>('first');
const newGameEl = byId<HTMLButtonElement>('new-game');
const undoEl = byId<HTMLButtonElement>('undo');
const statsEl = byId<HTMLElement>('stats');

DIFFICULTIES.forEach((name, i) => difficultyEl.add(new Option(name, String(i), i === 1, i === 1)));

// One button per column, each holding six cells. Built once so a focused column keeps focus
// across renders. cells[col][row], row 0 at the bottom.
const columnEls: HTMLButtonElement[] = [];
const cells: HTMLDivElement[][] = [];
for (let col = 0; col < COLS; col++) {
  const button = document.createElement('button');
  button.className = 'column';
  button.addEventListener('click', () => playHuman(col));
  cells.push([]);
  for (let row = 0; row < ROWS; row++) {
    const cell = document.createElement('div');
    cell.className = 'cell';
    button.append(cell); // the CSS stacks them bottom-up
    cells[col].push(cell);
  }
  columnEls.push(button);
  boardEl.append(button);
}

// ---------------------------------------------------------------------------------------------
// Rendering
// ---------------------------------------------------------------------------------------------

function render(): void {
  const board = grid(moves);
  const allowed = humanCanMove();
  const won = winMask !== 0;
  boardEl.className = `board human-p${human}${won ? ' won' : ''}`;
  boardEl.setAttribute('aria-busy', String(busy));

  for (let col = 0; col < COLS; col++) {
    const h = height(moves, col);
    const open = canPlay(moves, col);
    // aria-disabled instead of disabled: a disabled button drops keyboard focus, which would
    // throw the arrow-key position away every time the engine thinks.
    columnEls[col].setAttribute('aria-disabled', String(!allowed || !open));
    columnEls[col].setAttribute('aria-label', `Column ${col + 1}, ${open ? `${ROWS - h} empty` : 'full'}`);

    for (let row = 0; row < ROWS; row++) {
      const cell = cells[col][row];
      cell.classList.toggle('target', allowed && row === h);
      const owner = board[col][row];
      if (owner === 0) {
        cell.replaceChildren();
        continue;
      }
      // Existing discs are kept, so a disc element is only created when its move is added.
      // That makes every new disc drop in (its CSS animation runs once, on creation), while
      // re-renders, undo and new game never replay old drops.
      let disc = cell.firstElementChild as HTMLDivElement | null;
      if (!disc) {
        disc = document.createElement('div');
        disc.className = `disc p${owner}`;
        disc.style.setProperty('--fall', String(ROWS - row)); // rows to fall from above the board
        cell.append(disc);
      }
      disc.classList.toggle('win', won && inMask(winMask, col, row));
    }
  }

  spinnerEl.hidden = !thinking;
  statusEl.textContent = statusText();
  newGameEl.disabled = busy;
  undoEl.disabled = busy || !canUndo(moves, human);
  renderStats();
}

function statusText(): string {
  const you = human === 1 ? 'red' : 'yellow';
  if (error) return `Engine error: ${error}. Start a new game to try again.`;
  if (winMask !== 0) return playerToMove(moves) === human ? 'The engine wins.' : 'You win!';
  if (isBoardFull(moves)) return 'Draw: the board is full.';
  if (thinking) return `${DIFFICULTIES[Number(difficultyEl.value)]} engine is thinking...`;
  if (busy) return statusEl.textContent ?? ''; // a brief wait between moves: keep the last message
  return `Your turn (${you}). Pick a column.`;
}

function renderStats(): void {
  statsEl.hidden = lastMove === null;
  if (lastMove === null) return;
  byId('stat-column').textContent = String(lastMove.column + 1);
  byId('stat-depth').textContent = plies(lastMove.depth);
  byId('stat-nodes').textContent = lastMove.nodes.toLocaleString('en-US');
  // The engine reports 0 nodes/s when its timer read 0 ms, which happens at Easy.
  byId('stat-nps').textContent =
    lastMove.timeMs > 0 ? `${Math.round(lastMove.nps).toLocaleString('en-US')} nodes/s` : 'too fast to measure';
  byId('stat-time').textContent = `${lastMove.timeMs.toFixed(1)} ms`;
  byId('stat-score').textContent = `${describeScore(lastMove.score)} (score ${lastMove.score})`;
}

// Heuristic bands are in evaluation units: a center stone is worth 3 and an open three 5
// (EvalWeights in engine/include/c4/eval.hpp).
function describeScore(score: number): string {
  // Mate distances count from before the engine's move, so the engine's own move is ply 1.
  if (score >= MIN_WIN_SCORE) return `engine sees a forced win in ${plies(WIN_SCORE - score)}`;
  if (score <= -MIN_WIN_SCORE) return `engine sees you can force a win in ${plies(WIN_SCORE + score)}`;
  const size = Math.abs(score);
  const leader = score > 0 ? 'engine' : 'you';
  if (size <= 2) return 'about even';
  if (size <= 9) return `${leader} slightly ahead`;
  if (size <= 24) return `${leader} ahead`;
  return `${leader} clearly ahead`;
}

function plies(n: number): string {
  return n === 1 ? '1 ply' : `${n} plies`;
}

// ---------------------------------------------------------------------------------------------
// Game flow
// ---------------------------------------------------------------------------------------------

function sleep(ms: number): Promise<void> {
  return new Promise((resolve) => setTimeout(resolve, ms));
}

// Runs one step that waits on the engine, with input blocked until it finishes.
async function withEngine(step: () => Promise<void>): Promise<void> {
  busy = true;
  render();
  try {
    await step();
  } catch (e) {
    error = e instanceof Error ? e.message : String(e);
  }
  busy = false;
  thinking = false;
  render();
}

// Adds a move, then asks the engine whether it won the game.
async function addMove(col: number): Promise<void> {
  moves = [...moves, col];
  render();
  winMask = await winningCells(toMoveString(moves));
}

async function engineTurn(): Promise<void> {
  thinking = true;
  render();
  const started = performance.now();
  const move = await chooseMove(toMoveString(moves), Number(difficultyEl.value));
  await sleep(MIN_THINKING_MS - (performance.now() - started));
  thinking = false;
  lastMove = move;
  await addMove(move.column);
}

function playHuman(col: number): void {
  if (!humanCanMove() || !canPlay(moves, col)) return;
  void withEngine(async () => {
    await addMove(col);
    if (!gameOver()) await engineTurn();
  });
}

function newGame(): void {
  if (busy) return;
  moves = [];
  human = Number(firstEl.value) as Player;
  winMask = 0;
  lastMove = null;
  error = '';
  if (human === 2) void withEngine(engineTurn);
  else render();
}

function undoPair(): void {
  if (busy || !canUndo(moves, human)) return;
  moves = undo(moves, human);
  winMask = 0; // a game ends at its winning move, so any earlier position is not won
  lastMove = null; // the stats described a move that is gone
  error = '';
  render();
}

// ---------------------------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------------------------

newGameEl.addEventListener('click', newGame);
undoEl.addEventListener('click', undoPair);
// The thinking message names the level, so keep it current. A new level applies from the
// engine's next move; "first move" applies from the next new game.
difficultyEl.addEventListener('change', () => render());

// Keys 1 to 7 play that column. Left and right arrows move focus between columns, and Enter
// (or Space) presses the focused column, as for any button.
document.addEventListener('keydown', (event) => {
  // The selects use digits and arrows themselves; modifier combinations belong to the browser.
  if (event.target instanceof HTMLSelectElement || event.ctrlKey || event.metaKey || event.altKey) return;
  if (event.key >= '1' && event.key <= '7') {
    const col = Number(event.key) - 1;
    columnEls[col].focus();
    playHuman(col);
  } else if (event.key === 'ArrowLeft' || event.key === 'ArrowRight') {
    const current = columnEls.indexOf(document.activeElement as HTMLButtonElement);
    const step = event.key === 'ArrowLeft' ? -1 : 1;
    const next = current < 0 ? 3 : Math.min(COLS - 1, Math.max(0, current + step));
    columnEls[next].focus();
    event.preventDefault();
  }
});

newGame();
