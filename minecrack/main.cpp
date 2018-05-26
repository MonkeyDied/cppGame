#include <cstdio>
#include <set>
#include <vector>
#include <chrono>
#include <cmath>
#include <thread>
#include <array>
#include <iostream>
#include "sysapi.hpp"
#include "fmt/format.h"
#include "fmt/printf.h"

void wait(unsigned int ms) {
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}
const int nX = 30;
const int nY = 16;
using grid_t = std::array<std::array<int, nY>, nX>;
int serial_index(int x, int y) { return y*nX + x; }
void from_serial(int s, int& x, int& y) {
	x = s % nX;
	y = s / nX;
}
struct GridIter {
	GridIter(int cX = 0, int cY = 0) :
		_x(cX), _y(cY) 
	{
		reset();
	}

	int& at(grid_t& grid) {
		return grid[x()][y()];
	}
	int x() const { return _x + i; }
	int y() const { return _y + j; }
	void reset() {
		i = j = -1;
		while (x() < 0) ++i;
		while (y() < 0) ++j;
	}
	
	bool next() {
		if (i > 1 || x() >= nX) 
			return false;
		j = j + 1;
		if (j > 1 || y() >= nY) {
			j = -1;
			while (y() < 0) ++j;
			i = i + 1;
			if (i > 1 || x() >= nX)
				return false;
		}
		return true;
	}

	int _x, _y, i, j;
};

enum {
	TileError = -3,
	TileBoom = -2,
	TileUnknown = -1,	
	TileMine = -4,	// An Unknown tile deduced to be mine
	TileSafe = -5,	// An Unknown tile deduced to be safe
	TileMNew = -6,  // An Unknown tile deduced to be mine (newly added ones)
	TileZero = 0
};
int basicTileType(int type) {
	if (type == TileMine || type == TileSafe || type == TileMNew) 
		return TileUnknown;
	return type;
}
bool isUnknown(int type) {
	return basicTileType(type) == TileUnknown;
}
char tileChar(int type) {
	char c;
	switch (type) {
	case TileError:   c = '!'; break;
	case TileBoom:    c = '*'; break;
	case TileUnknown: c = '-'; break;
	case TileZero:    c = ' '; break;
	case TileMine:    c = 'm'; break;
	case TileMNew:    c = 't'; break;
	case TileSafe:    c = 'o'; break;
	default:
		c = '0' + type;
		break;
	}
	return c;
}
void print_grid(grid_t& grid) {
	fmt::print("  ");
	for (int i = 0; i < nX; ++i){
		fmt::print("{} ", (i % 10));
	}
	fmt::print("\n");
	for (int j = 0; j < nY; ++j) {
		fmt::print("{} ", (j % 10));
		for (int i = 0; i < nX; ++i){
			fmt::print("{} ", tileChar(grid[i][j]));
		}
		fmt::print("\n");
	}
	fmt::print("<--Grid-->\n");
}

class MineS {
public:
	void sim_click(int i, int j) const {
		mouseMoveTo(toX(i), toY(j));
		mouseLeftClick();
	}
	int click(int i, int j) {
		sim_click(i, j);
		wait(30);	// wait for the screen to refresh
		return tile(i, j);
	}
	int click(int index) {
		int x, y;
		from_serial(index, x, y);
		return click(x, y);
	}
	int tile(int i, int j) const {
		unsigned int c_mine = item.back();
		unsigned int c = getScreenColor(toX(i), toY(j));
		if (c == item[0]) {
			// need further check
			unsigned int c2 = getScreenColor(toX(i), toY(j) - 7);
			//fmt::print("Color below 8 {:x}\n", c2);
			if (c2 == 0xffffff){
				return TileUnknown;
			}
			c2 = getScreenColor(toX(i) + 1, toY(j));
			if (c2 == 0) {
				return 7;
			}
			return TileZero;
		}
		else {
			auto it = std::find(item.begin(), item.end(), c);
			if (it == item.end()) {
				fmt::print("Warn: Find an unknown color {:x}, at i/j={}/{}\n", c, i, j);
				return TileError;
			}
			if (c == c_mine)
				return TileBoom;
			return it - item.begin();
		}
	}

	unsigned int color(int i, int j) const {
		return getScreenColor(toX(i), toY(j));
	}

	bool good() const {
		unsigned int c = getScreenColor(rect.left + smile[0], rect.top + smile[1]);
		if (c == 0xffff) 
			return true;
		return false;
	}
	void reset() {
		mouseMoveTo(rect.left + smile[0], rect.top + smile[1]);
		mouseLeftClick();
		reset_grid();
		wait(50);
	}
	void reset_grid() {
		for (auto& one : grid) {
			one.fill(TileUnknown);
		}
	}
	bool refresh_rect() {
		return getWindowRect(rect);
	}
	void refresh_grid() {
		captureScreen();
		// read screen pxiels
		for (int i = 0; i < nX; ++i) {
			auto& col = grid[i];
			for (int j = 0; j < nY; ++j){
				int c = tile(i, j);
				col[j] = c;
			}
		}
	}
	
	void show_grid() {
		print_grid(grid);
	}

	Rect rect;
	grid_t grid;

	int toX(int i) const { 
		return rect.left + base[0] + dq * i; 
	}
	int toY(int j) const { 
		return rect.top + base[1] + dq * j; 
	}

	static const int base[2];
	static const int smile[2];
	static const std::vector<unsigned int> item;
	static const int dq = 16;
};
const int MineS::base[2] = { 47 - 24, 201 - 92 };
const int MineS::smile[2] = { 279 - 24, 167 - 92 };
const std::vector<unsigned int> MineS::item = {
	0xc0c0c0,
	0xff0000,
	0x8000,
	0xff,
	0x800000,
	0x80,
	0x808000,
	0x77,
	0		// This is when clicking a mine
};

// The algorithm
class Algo {
public:
	struct Stats {
		int nMine = 0;
		int nSafe = 0;
		int nMNew = 0;
		int nUnknown = 0;
	};
	Algo() {
		reset();
	}

	void reset() {
		for (auto& one : grid) {
			one.fill(TileUnknown);
		}
	}

	bool update(grid_t& raw) {
		for (int i = 0; i < nX; ++i) {
			auto& col_raw = raw[i];
			auto& col = grid[i];
			for (int j = 0; j < nY; ++j) {
				if (col_raw[j] != basicTileType(col[j])) {
					if (col_raw[j] == TileError) {
						fmt::print("Error: Get an tile of TypeError!\n");
						return false;
					}
					if (col_raw[j] == TileBoom) {
						fmt::print("Warning: Get a tile of TypeBoom!\n");
						return false;
					}
					if (col_raw[j] >= 0 && col[j] >= 0) {
						// This is known tile, but value may be updated
						// So we won't update the grid
					}
					else {
						col[j] = col_raw[j];
						Stats s = stats(i, j);
						col[j] -= s.nMine;
					}
				}
			}
		}
		return true;
	}

	// Phase One
	bool analysis(std::set<int>& safe, std::set<int>& mine) {
		int nchanged = 0;
		for (int i = 0; i < nX; ++i) {
			auto& col = grid[i];
			for (int j = 0; j < nY; ++j) {
				// col[j] contains the number of the remaining undetected mines
				if (col[j] > 0) {
					Stats s = stats(i, j);
					if (col[j] == s.nUnknown) {
						// The rest are mine
						auto unMe = unknownOf(i, j);
						for (auto one : unMe) {
							flagMine(one);
							mine.insert(one);
							nchanged++;
						}
						//mark(i, j, TileMNew, &mine);
						col[j] = 0;	// This tile is done
					}
					else if (col[j] < s.nMNew) {
						// This should not happen
						fmt::print("Error: Mine count < s.nMNew\n");
					}
				}
				else if (col[j] == 0) {
					auto unMe = unknownOf(i, j);
					for (auto one : unMe) {
						flagSafe(one);
						safe.insert(one);
						nchanged++;
					}
				}
			}
		}

		return nchanged > 0;
	}

	// Auxiliary functions: unknownOf, borderOf, nebExcept, nebInside
	int unknownOf(int i, int j, std::set<int>& out) {
		GridIter iter(i, j);
		int n = 0;
		do {
			if (iter.at(grid) == TileUnknown) {
				out.insert(serial_index(iter.x(), iter.y()));
				++n;
			}
		}while (iter.next());
		return n;
	}
	std::set<int> unknownOf(int i, int j) { 
		std::set<int> out; 
		unknownOf(i, j, out);
		return out;
	}

	int borderOf(int i, int j, std::set<int>& out) {
		GridIter iter(i, j);
		int n = 0;
		do {
			if (iter.at(grid) > 0) {
				out.insert(serial_index(iter.x(), iter.y()));
				++n;
			}
		} while (iter.next());
		return n;
	}
	std::set<int> borderOf(int i, int j) {
		std::set<int> out;
		borderOf(i, j, out);
		return out;
	}

	int nebExcept(int i, int j, const std::set<int>& except, std::set<int>& out) {
		GridIter iter(i, j);
		int n = 0;
		do {
			int index = serial_index(iter.x(), iter.y());
			if (except.find(index) == except.end()) {
				out.insert(index);
				++n;
			}
		} while (iter.next());
		return n;
	}
	std::set<int> nebExcept(int i, int j, const std::set<int>& except) {
		std::set<int> out;
		nebExcept(i, j, except, out);
		return out;
	}

	int nebInside(int i, int j, const std::set<int>& range, std::set<int>& out) {
		GridIter iter(i, j);
		int n = 0;
		do {
			int index = serial_index(iter.x(), iter.y());
			if (range.find(index) != range.end()) {
				out.insert(index);
				++n;
			}
		} while (iter.next());
		return n;
	}
	std::set<int> nebInside(int i, int j, const std::set<int>& range) {
		std::set<int> out;
		nebInside(i, j, range, out);
		return out;
	}

	int unknownExcept(int i, int j, const std::set<int>& except, std::set<int>& out) {
		GridIter iter(i, j);
		int n = 0;
		do {
			int index = serial_index(iter.x(), iter.y());
			if (iter.at(grid) == TileUnknown && except.find(index) == except.end()) {
				out.insert(index);
				++n;
			}
		} while (iter.next());
		return n;
	}
	std::set<int> unknownExcept(int i, int j, const std::set<int>& except) {
		std::set<int> out;
		unknownExcept(i, j, except, out);
		return out;
	}

	int unknownInside(int i, int j, const std::set<int>& range, std::set<int>& out) {
		GridIter iter(i, j);
		int n = 0;
		do {
			int index = serial_index(iter.x(), iter.y());
			if (iter.at(grid) == TileUnknown && range.find(index) != range.end()) {
				out.insert(index);
				++n;
			}
		} while (iter.next());
		return n;
	}
	std::set<int> unknownInside(int i, int j, const std::set<int>& range) {
		std::set<int> out;
		unknownInside(i, j, range, out);
		return out;
	}

	void merge(std::set<int>& to, const std::set<int>& from) {
		for (auto x : from) {
			to.insert(x);
		}
	}

	bool analysis_ex(std::set<int>& safe, std::set<int>& mine) {
		int nchanged = 0;
		for (int i = 0; i < nX; ++i) {
			auto& col = grid[i];
			for (int j = 0; j < nY; ++j) {
				if (col[j] > 0) {
					// This one is an unknown
					// ub(unMutual.mines) = min(|unMutual.mines|, it.remain)
					// lb(ubMutual.mines) = max(0, it.remain - |unIt|)
					// 
					auto unMe = unknownOf(i, j);
					int meRemain = grid[i][j];
					if (meRemain == unMe.size()) {
						for (auto one : unMe) {
							flagMine(one);
							nchanged++;
						}
						continue;
					}
					for (auto idx_border : borderOf(i, j)) {
						int i2, j2;
						from_serial(idx_border, i2, j2);
						if (grid[i2][j2] == 0) {
							for (auto one : unknownOf(i2, j2)) {
								flagSafe(one);
								nchanged++;
							}
						}

						auto unMutual   = unknownInside(i2, j2, unMe);
						auto unItExcept = unknownExcept(i2, j2, unMutual);
						auto unMeExcept = unknownExcept(i, j, unMutual);

						int itRemain = grid[i2][j2];
						int ub = std::min((int)unMutual.size(), itRemain);
						int lb = std::max(0, itRemain - (int)unItExcept.size());

						if (ub + unMeExcept.size() == meRemain) {
							// All in unMeExcept are mines
							for (auto one : unMeExcept) {
								flagMine(one);
								mine.insert(one);
								nchanged++;
							}

							if (unMutual.size() >= itRemain) {
								// And all in unItExcept may be safe
								// Note unMutual.size() should always > itRemain if phase 1 is properly done
								for (auto one : unItExcept) {
									flagSafe(one);
									safe.insert(one);
									nchanged++;
								}
								if (unMutual.size() == itRemain) {
									for (auto one : unMutual) {
										flagMine(one);
										mine.insert(one);
										nchanged++;
									}
								}
							}

							// Refresh stats of me
							unMe = unknownOf(i, j);
							meRemain = grid[i][j];
						}
						else if (lb == meRemain) {
							// All in unMeExcept are safe
							for (auto one : unMeExcept) {
								flagSafe(one);
								safe.insert(one);
								nchanged++;
							}
							if (unItExcept.size() <= itRemain) {
								// and all in unItExcept may be mines
								for (auto one : unItExcept) {
									flagMine(one);
									mine.insert(one);
									nchanged++;
								}
								if (unItExcept.size() == itRemain) {
									for (auto one : unMutual) {
										flagSafe(one);
										safe.insert(one);
										nchanged++;
									}
								}
							}

							// Refresh stats of me
							unMe = unknownOf(i, j);
							meRemain = grid[i][j];
						}
					}
				}
			}
		}
		return nchanged > 0;
	}

	bool analysis_guess(std::set<int>& safe, std::set<int>& mine, int max_guess = 2) {
		// TODO: add some mechanism to guess mines when you cant do it by deduction
	}
	
	void flagSafe(int index) {
		int i, j;
		from_serial(index, i, j);
		grid[i][j] = TileSafe;
	}

	void flagMine(int i, int j) {
		if (basicTileType(grid[i][j]) == TileUnknown) {
			if (grid[i][j] != TileMine) {
				grid[i][j] = TileMine;
				
				for (auto index : borderOf(i, j)) {
					int x, y;
					from_serial(index, x, y);
					if (grid[x][y] == TileZero) {
						fmt::print("Error: Flag a mine leads to incompatible mine count!\n");
					}
					else if (grid[x][y] > 0) {
						grid[x][y]--;
					}
				}
			}
		}
	}
	void flagMine(int index) {
		int x, y;
		from_serial(index, x, y);
		flagMine(x, y);
	}

	int mark(int i, int j, int val, std::set<int>* to = nullptr, int cond = TileUnknown) {
		int n = 0;
		for (int a = -1; a <= 1; ++a){
			int x = i + a;
			if (x >= 0 && x < nX) {
				for (int b = -1; b <= 1; ++b) {
					int y = j + b;
					if (y >= 0 && y < nY) {
						if (grid[x][y] == cond) {
							grid[x][y] = val;
							n++;
							if (to) {
								to->insert(serial_index(x, y));
							}
						}
					}
				}
			}
		}
		return n;
	}

	Stats stats(int i, int j, grid_t* g = nullptr) {
		Stats s;
		if (!g) g = &grid;
		for (int a = -1; a <= 1; ++a){
			int x = i + a;
			if (x >= 0 && x < nX) {
				for (int b = -1; b <= 1; ++b) {
					int y = j + b;
					if (y >= 0 && y < nY) {
						int type = (*g)[x][y];
						if (basicTileType(type) == TileUnknown) {
							if (type == TileSafe)    s.nSafe++;
							if (type == TileMine)    s.nMine++;
							if (type == TileMNew)    s.nMNew++;
							if (type == TileUnknown) s.nUnknown++;
						}
					}
				}
			}
		}
		return s;
	}

	std::set<int> last_safe;
	std::set<int> last_mine;
	grid_t grid;
};

#include <iostream>
using std::cin;
int main() {
	auxiliary_init();
	MineS mine;
	if (!mine.refresh_rect()) {
		puts("Failed to get Window Rect of Arbiter");
		system("pause");
		return -1;
	}

	// Play the game
	Algo algo;
	std::string cmd;
	std::set<int> tile_mine, tile_safe;
	while (1) {
		printf("Input cmd: ");
		cin >> cmd;
		if (cmd == "q") break;
		if (cmd == "s0") {
			print_grid(mine.grid);
		}
		if (cmd == "s1") {
			print_grid(algo.grid);
		}
		if (cmd == "r") {
			mine.refresh_grid();
			algo.update(mine.grid);
		}
		if (cmd == "rr") {
			mine.reset_grid();
			algo.reset();
			mine.refresh_grid();
			algo.update(mine.grid);
		}
		if (cmd == "p0") {
			tile_mine.clear();
			tile_safe.clear();
			if (algo.analysis(tile_safe, tile_mine)) {
				fmt::print("From Analysis, detected {} mines, {} safe tiles.\n", 
					tile_mine.size(), tile_safe.size());

				print_grid(algo.grid);
			}
			else {
				fmt::print("No new information deduced from Analysis\n");
			}
		}
		if (cmd== "p1") {
			tile_mine.clear();
			tile_safe.clear();
			if (algo.analysis_ex(tile_safe, tile_mine)) {
				fmt::print("From AnalysisEx, detected {} mines, {} safe tiles.\n",
					tile_mine.size(), tile_safe.size());

				print_grid(algo.grid);
			}
			else {
				fmt::print("No new information deduced from AnalysisEx\n");
			}
		}
		if (cmd == "go") {
			int n = 0;
			for (int i = 0; i < nX; ++i) {
				for (int j = 0; j < nY; ++j) {
					if (algo.grid[i][j] == TileSafe) {
						mine.click(i, j);
						n++;
					}
				}
			}
			fmt::print("Done clicking {} safe tiles.\n", n);
			//mine.refresh_grid();
			//algo.update(mine.grid);
		}
		if (cmd == "play") {
			mine.reset_grid();
			algo.reset();
			while (1) {
				mine.refresh_grid();
				algo.update(mine.grid);
				
				tile_mine.clear();
				tile_safe.clear();
				while (algo.analysis(tile_safe, tile_mine)) {}
				while (algo.analysis_ex(tile_safe, tile_mine)) {}

				fmt::print("Detected {} mines, {} safe tiles.\n", tile_mine.size(), tile_safe.size());
				if (tile_mine.size() == 0 && tile_safe.size() == 0) {
					fmt::print("No new info revealed. Done.\n");
					break;
				}

				for (int i = 0; i < nX; ++i) {
					for (int j = 0; j < nY; ++j) {
						if (algo.grid[i][j] == TileSafe) {
							mine.click(i, j);
						}
					}
				}
				//wait(100);
			}
			//if (!mine.good()) {
			//	puts("Err... Something wrong, the game is over.\n");
			//}
		}
	}

	auxiliary_exit();
	return 0;
}


int main_1() {
	MineS mine;
	auxiliary_init();
	Rect& rect = mine.rect;
	if (!getWindowRect(rect)) {
		puts("Failed to get Window Rect of Arbiter");
	}
	else {
		fmt::print("WinRect: L/R/T/B, {}/{}/{}/{}\n",
			rect.left, rect.right, rect.top, rect.bottom);
	}

	int x, y;
	getMousePosition(x, y);
	fmt::print("Mouse: x/y, {}/{}\n", x, y);

	while (1) {
		char c = getchar();
		if (c == 'c') {
			puts("Capturing screen");
			captureScreen();
		}
		if (c == 'p') {
			getMousePosition(x, y);
			
			int dx = x - rect.left - MineS::base[0];
			int dy = y - rect.top - MineS::base[1];
			int nx = int(dx*1.0 / MineS::dq + 0.5);
			int ny = int(dy*1.0 / MineS::dq + 0.5);
			unsigned int color = mine.color(nx, ny);
			unsigned int color1 = getScreenColor(x, y);
			int tile = mine.tile(nx, ny);

			fmt::print("Mouse: x/y, {}/{}, at Tile {}, {}, Color {:x}, color {:x}, status: {}, tile: {}\n", 
				x, y, nx, ny, color1, color, (mine.good()? "good": "bad"), tile);
		}
		if (c == 'm') {
			getMousePosition(x, y);

			int dx = x - rect.left - MineS::base[0];
			int dy = y - rect.top - MineS::base[1];
			int nx = int(dx*1.0 / MineS::dq + 0.5);
			int ny = int(dy*1.0 / MineS::dq + 0.5);
			mine.click(nx, ny);
			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			unsigned int color = mine.color(nx, ny);
			fmt::print("Color = {:x}\n", color);
		}
		if (c == 's') {
			mine.refresh_grid();
			puts("Showing");
			mine.show_grid();
		}
		if (c == 't' || c == 'T' || c == 'Z') {
			int num = (c == 't') ? 5 : 
					  (c == 'T') ? 100 : 1000;
			// 0 is mine
			std::vector<unsigned> data = { 0xc0c0c0,	0xff0000,		0x8000,		0xff,	0x800000, 0x80, 0 };
			for (int i = 0; i < num; ++i) {
				int nx = rand() % 30;
				int ny = rand() % 16;
				mine.click(nx, ny);
				wait(30);
				if (!mine.good()) {
					mine.reset();
					wait(100);
				}
				else {
					unsigned int color = mine.color(nx, ny);
					auto iter = std::find(data.begin(), data.end(), color);
					if (iter == data.end()) {
						fmt::print("New item found! color = {}\n", color);
						c = getchar();
						if (c == 'r') {
							puts("Continuing");
							data.push_back(color);
						}
						else {
							break;
						}
					}
					else {
						fmt::print("Found item {}\n", iter - data.begin());
					}
				}
			}
			puts("Done.");
		}
		if (c == 'q') {
			break;
		}
	}

	auxiliary_exit();
	return 0;
}