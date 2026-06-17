# Maze Runner

A terminal-based maze game written in C++ to demonstrate core DSA (Data
Structures & Algorithms) and OOP (Object-Oriented Programming) concepts
through an actual playable project rather than isolated exercises.

Every time you run it, a brand new maze is generated using a randomized
version of **Kruskal's algorithm**, powered by a **Union-Find (Disjoint
Set)** data structure. You then navigate it in real time using W/A/S/D,
with a live move counter and timer, and a win screen once you reach the
exit.

## Requirements

- A C++17-capable compiler (`g++` recommended)
- A POSIX-compliant terminal (Linux, macOS, or WSL on Windows)

This project uses `termios.h` for real-time keyboard input (reading a
keypress instantly, without waiting for Enter). That header is
Linux/macOS-only, so **on Windows you must run this inside WSL**
(Windows Subsystem for Linux) — it will not compile with a native
Windows compiler like MinGW or MSVC.

## Building and Running

From a WSL or Linux/macOS terminal, inside the project folder:

```bash
g++ -std=c++17 -O2 -o maze_runner maze_runner.cpp
./maze_runner
```

If `g++` is not installed (common on a fresh WSL Ubuntu install):

```bash
sudo apt update && sudo apt install g++
```

## Controls

| Key | Action       |
|-----|--------------|
| W   | Move up      |
| A   | Move left    |
| S   | Move down    |
| D   | Move right   |
| Q   | Quit early   |

Movement is real-time — no need to press Enter. Moving into a wall is
simply ignored; the maze redraws and waits for your next keypress.

## How a New Maze Is Generated Every Time

The maze is built using a randomized spanning-tree construction:

1. Every possible wall between two adjacent cells is treated as a
   candidate **edge**.
2. That full list of edges is **shuffled** using `std::mt19937`, seeded
   from the current system time in nanoseconds — so the shuffle order
   is different on every run.
3. The shuffled edges are processed one by one. An edge is **accepted**
   (its wall removed) only if the two cells it connects are not already
   connected to each other, which is checked using **Union-Find**.
4. This continues until exactly `(rows * cols) - 1` edges have been
   accepted — the exact number needed for a spanning tree.

The Union-Find structure is what *guarantees* the result is always a
single connected maze with no loops and no isolated rooms, no matter
what order the edges are processed in. The random shuffle is what
makes *which* walls come down different every time, which is why the
maze layout changes from run to run while always remaining solvable.

If you swap the time-based seed for a fixed one (e.g. `mt19937
rng(42);`), the maze becomes fully reproducible — useful for debugging
or for giving two players the identical maze to race on.

## Project Structure & Concepts

| Class/Struct      | Responsibility                                                         |
|--------------------|-------------------------------------------------------------------------|
| `DisjointSet`       | Union-Find with path compression + union by rank — the core DSA piece  |
| `Edge` / `Cell`     | Plain data representations of a wall-candidate and a maze cell         |
| `Maze`              | Owns the grid; generates it via Kruskal's; encapsulates all wall logic |
| `Player`            | Tracks position and move count                                         |
| `TerminalRawMode`   | RAII wrapper that enables instant keypress reading and restores the terminal on exit |
| `Game`              | Orchestrates the loop: rendering, input handling, timer, win screen    |

OOP principles in play:

- **Encapsulation** — nothing outside `Maze` touches the raw grid or
  wall booleans directly; everything goes through methods like
  `canMove()`.
- **Single responsibility** — generation, player state, input handling,
  and game orchestration are each isolated in their own class.
- **RAII** — `TerminalRawMode` guarantees your terminal settings are
  restored even if the program exits unexpectedly, by doing the
  cleanup in its destructor.

## Possible Extensions

- An auto-solver visualization using BFS or DFS, showing the algorithm
  explore the maze cell by cell.
- An undo system using a custom linked list to step back through moves.
- A leaderboard using a heap to track best times across multiple runs.

## License

Free to use, modify, and extend for learning or coursework purposes.
