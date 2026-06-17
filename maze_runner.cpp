// ============================================================
// MAZE RUNNER
// A terminal-based maze game demonstrating DSA + OOP concepts:
//   - Union-Find (Disjoint Set) with union by rank + path compression
//   - Kruskal's algorithm adapted for randomized maze generation
//   - OOP: encapsulation, classes for Cell/Edge/Maze/Player/Game
//   - Real-time raw-mode keyboard input (no Enter key needed)
// ============================================================

#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <chrono>
#include <termios.h>
#include <unistd.h>
#include <fcntl.h>

using namespace std;
using namespace std::chrono;

// ------------------------------------------------------------
// DisjointSet: classic Union-Find with path compression and
// union by rank. This is the DSA backbone of maze generation —
// it lets us check "are these two rooms already connected?" in
// near O(1) time, which is exactly what Kruskal's needs.
// ------------------------------------------------------------
class DisjointSet {
private:
    vector<int> parent;
    vector<int> rank_;

public:
    explicit DisjointSet(int n) : parent(n), rank_(n, 0) {
        for (int i = 0; i < n; i++) parent[i] = i;
    }

    int find(int x) {
        if (parent[x] != x) {
            parent[x] = find(parent[x]); // path compression
        }
        return parent[x];
    }

    // Returns true if a union happened (they were in different sets)
    bool unite(int a, int b) {
        int ra = find(a), rb = find(b);
        if (ra == rb) return false;

        if (rank_[ra] < rank_[rb]) swap(ra, rb);
        parent[rb] = ra;
        if (rank_[ra] == rank_[rb]) rank_[ra]++;
        return true;
    }
};

// ------------------------------------------------------------
// Edge: represents a potential wall between two adjacent cells.
// Kruskal's algorithm normally picks edges by weight; here we
// randomize the edge order instead, which turns "minimum
// spanning tree" into "random spanning tree" — ideal for maze
// generation, since it guarantees a fully connected maze with
// exactly one path between any two cells (no loops).
// ------------------------------------------------------------
struct Edge {
    int u, v; // cell indices
};

// ------------------------------------------------------------
// Cell: a single maze cell. Walls are tracked individually so
// rendering can draw a proper grid.
// ------------------------------------------------------------
struct Cell {
    bool wallTop = true, wallBottom = true, wallLeft = true, wallRight = true;
    bool visited = false; // used only for solver/visited-trail rendering
};

// ------------------------------------------------------------
// Maze: owns the grid, builds it via Kruskal's algorithm using
// DisjointSet, and exposes movement-validity checks. Encapsulation
// in action: nothing outside this class touches the raw grid.
// ------------------------------------------------------------
class Maze {
private:
    int rows, cols;
    vector<Cell> grid;

    int idx(int r, int c) const { return r * cols + c; }

    void removeWallBetween(int r1, int c1, int r2, int c2) {
        if (r1 == r2 && c1 + 1 == c2) { // c1 is left of c2
            grid[idx(r1, c1)].wallRight = false;
            grid[idx(r2, c2)].wallLeft = false;
        } else if (r1 == r2 && c1 - 1 == c2) { // c1 is right of c2
            grid[idx(r1, c1)].wallLeft = false;
            grid[idx(r2, c2)].wallRight = false;
        } else if (c1 == c2 && r1 + 1 == r2) { // r1 is above r2
            grid[idx(r1, c1)].wallBottom = false;
            grid[idx(r2, c2)].wallTop = false;
        } else if (c1 == c2 && r1 - 1 == r2) { // r1 is below r2
            grid[idx(r1, c1)].wallTop = false;
            grid[idx(r2, c2)].wallBottom = false;
        }
    }

public:
    Maze(int rows_, int cols_) : rows(rows_), cols(cols_), grid(rows_ * cols_) {
        generate();
    }

    int getRows() const { return rows; }
    int getCols() const { return cols; }

    // Kruskal's-style generation using Union-Find.
    void generate() {
        vector<Edge> edges;
        edges.reserve(rows * cols * 2);

        // Build every possible edge (right neighbor + bottom neighbor)
        for (int r = 0; r < rows; r++) {
            for (int c = 0; c < cols; c++) {
                if (c + 1 < cols) edges.push_back({idx(r, c), idx(r, c + 1)});
                if (r + 1 < rows) edges.push_back({idx(r, c), idx(r + 1, c)});
            }
        }

        // Shuffle edges randomly — this is what makes it a *random*
        // spanning tree instead of a *minimum* spanning tree.
        unsigned seed = chrono::system_clock::now().time_since_epoch().count();
        mt19937 rng(seed);
        shuffle(edges.begin(), edges.end(), rng);

        DisjointSet ds(rows * cols);
        int edgesUsed = 0;
        int needed = rows * cols - 1; // a spanning tree has (n-1) edges

        for (auto &e : edges) {
            if (edgesUsed == needed) break;
            if (ds.unite(e.u, e.v)) {
                int r1 = e.u / cols, c1 = e.u % cols;
                int r2 = e.v / cols, c2 = e.v % cols;
                removeWallBetween(r1, c1, r2, c2);
                edgesUsed++;
            }
        }
    }

    bool canMove(int r, int c, char dir) const {
        if (r < 0 || r >= rows || c < 0 || c >= cols) return false;
        const Cell &cell = grid[idx(r, c)];
        switch (dir) {
            case 'U': return !cell.wallTop;
            case 'D': return !cell.wallBottom;
            case 'L': return !cell.wallLeft;
            case 'R': return !cell.wallRight;
        }
        return false;
    }

    void markVisited(int r, int c) { grid[idx(r, c)].visited = true; }
    bool isVisited(int r, int c) const { return grid[idx(r, c)].visited; }

    const Cell &at(int r, int c) const { return grid[idx(r, c)]; }
};

// ------------------------------------------------------------
// Player: tracks position and move count. Kept separate from
// Maze and Game to respect single-responsibility.
// ------------------------------------------------------------
class Player {
private:
    int row, col;
    int moves = 0;

public:
    Player(int r, int c) : row(r), col(c) {}

    int getRow() const { return row; }
    int getCol() const { return col; }
    int getMoves() const { return moves; }

    void moveTo(int r, int c) {
        row = r;
        col = c;
        moves++;
    }
};

// ------------------------------------------------------------
// Raw terminal mode helpers: lets us read a single keypress
// without waiting for Enter and without echoing it to screen.
// ------------------------------------------------------------
class TerminalRawMode {
private:
    termios oldSettings;

public:
    TerminalRawMode() {
        termios newSettings;
        tcgetattr(STDIN_FILENO, &oldSettings);
        newSettings = oldSettings;
        newSettings.c_lflag &= ~(ICANON | ECHO); // disable line buffering & echo
        newSettings.c_cc[VMIN] = 1;
        newSettings.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &newSettings);
    }

    ~TerminalRawMode() {
        tcsetattr(STDIN_FILENO, TCSANOW, &oldSettings);
    }
};

// ------------------------------------------------------------
// Game: orchestrates everything — owns the Maze and Player,
// runs the input/render loop, tracks the timer, and shows the
// win screen. This is the only class that knows about all the
// others (composition over a tangle of dependencies).
// ------------------------------------------------------------
class Game {
private:
    Maze maze;
    Player player;
    int exitRow, exitCol;
    steady_clock::time_point startTime;

    void render() const {
        cout << "\x1B[2J\x1B[H"; // clear screen, move cursor to top

        cout << "MAZE RUNNER  |  Moves: " << player.getMoves()
             << "  |  Use W A S D to move, Q to quit\n\n";

        int rows = maze.getRows(), cols = maze.getCols();

        // Top border
        for (int c = 0; c < cols; c++) cout << "+--";
        cout << "+\n";

        for (int r = 0; r < rows; r++) {
            // Cell row: left wall + cell content + right wall
            cout << "|";
            for (int c = 0; c < cols; c++) {
                if (r == player.getRow() && c == player.getCol()) {
                    cout << " P";
                } else if (r == exitRow && c == exitCol) {
                    cout << " E";
                } else {
                    cout << "  ";
                }
                cout << (maze.at(r, c).wallRight ? "|" : " ");
            }
            cout << "\n";

            // Bottom wall row
            cout << "+";
            for (int c = 0; c < cols; c++) {
                cout << (maze.at(r, c).wallBottom ? "--" : "  ");
                cout << "+";
            }
            cout << "\n";
        }

        auto elapsed = duration_cast<seconds>(steady_clock::now() - startTime).count();
        cout << "\nTime: " << elapsed << "s\n";
    }

    void showWinScreen() const {
        auto elapsed = duration_cast<seconds>(steady_clock::now() - startTime).count();
        cout << "\x1B[2J\x1B[H";
        cout << "================================\n";
        cout << "        YOU REACHED THE EXIT!\n";
        cout << "================================\n";
        cout << "  Moves taken : " << player.getMoves() << "\n";
        cout << "  Time taken  : " << elapsed << " seconds\n";
        cout << "================================\n";
    }

public:
    Game(int rows, int cols)
        : maze(rows, cols), player(0, 0), exitRow(rows - 1), exitCol(cols - 1) {
        startTime = steady_clock::now();
    }

    void run() {
        TerminalRawMode raw; // RAII: restores terminal settings on destruction

        while (true) {
            render();

            if (player.getRow() == exitRow && player.getCol() == exitCol) {
                showWinScreen();
                break;
            }

            char key = getchar();
            int r = player.getRow(), c = player.getCol();

            if (key == 'q' || key == 'Q') {
                cout << "\nQuit early. Final moves: " << player.getMoves() << "\n";
                break;
            } else if ((key == 'w' || key == 'W') && maze.canMove(r, c, 'U')) {
                player.moveTo(r - 1, c);
            } else if ((key == 's' || key == 'S') && maze.canMove(r, c, 'D')) {
                player.moveTo(r + 1, c);
            } else if ((key == 'a' || key == 'A') && maze.canMove(r, c, 'L')) {
                player.moveTo(r, c - 1);
            } else if ((key == 'd' || key == 'D') && maze.canMove(r, c, 'R')) {
                player.moveTo(r, c + 1);
            }
            // Any other key, or a blocked move, is simply ignored
            // and the loop re-renders the same state.
        }
    }
};

int main() {
    int rows = 10, cols = 15;

    cout << "Generating maze (" << rows << "x" << cols << ")...\n";
    Game game(rows, cols);
    game.run();

    return 0;
}