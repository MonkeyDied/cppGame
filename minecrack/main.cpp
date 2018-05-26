#include <cstdio>
#include <set>
#include <vector>
#include <chrono>
#include <cmath>
#include <thread>
#include <array>
#include <iostream>
#include "sysapi.hpp"
#include "mine.hpp"
#include "fmt/format.h"
#include "fmt/printf.h"

// Some Utility functions about set operation
std::set<int> set_union(const std::set<int>& to, const std::set<int>& from) {
	std::set<int> ret = to;
	for (auto x : from) {
		ret.insert(x);
	}
	return ret;
}

std::set<int> set_diff(const std::set<int>& base_set, const std::set<int>& target) {
	std::set<int> base = base_set;
	auto it = base.begin();
	while (it != base.end()) {
		if (target.count(*it) != 0) {
			it = base.erase(it);
		}
		else {
			it++;
		}
	}
	return base;
}
std::set<int> set_intersect(const std::set<int>& base_set, const std::set<int>& target) {
	std::set<int> base = base_set;
	auto it = base.begin();
	while (it != base.end()) {
		if (target.count(*it) == 0) {
			it = base.erase(it);
		}
		else {
			it++;
		}
	}
	return base;
}

// The algorithm
class Algo {
public:
	struct Stats {
		int nMine = 0;
		int nSafe = 0;
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
		auto iter = whole_grid();
		do {
			int vNew = iter.at(raw);
			int& vMe = iter.at(grid);
			if (vNew != basicTileType(vMe)) {
				if (vNew == TileError) {
					fmt::print("Error: Get an tile of TypeError!\n");
					return false;
				}
				if (vNew == TileBoom) {
					fmt::print("Warning: Get a tile of TypeBoom!\n");
					return false;
				}
				if (vNew >= 0 && vMe >= 0) {
					// This is known tile, but value may be updated
					// So we won't update the grid
				}
				else {
					vMe = vNew;
					Stats s = stats(iter.x(), iter.y());
					vMe -= s.nMine;
				}
			}
		} while (iter.next());
		return true;
	}

	// Helper function
	std::set<int> unknownOf(int i, int j) {
		return find(grid, GridIter(i, j), TileUnknown);
	}
	std::set<int> borderOf(int i, int j, bool withZero = true) {
		return find(grid, GridIter(i, j), (withZero ? TileBorderWithZero : TileBorder));
	}

	// Phase One
	bool analysis(std::set<int>& safe, std::set<int>& mine) {
		int nchanged = 0;
		auto iter = whole_grid();
		do {
			int& remain = iter.at(grid);
			if (remain > 0) {
				Stats s = stats(iter.x(), iter.y());
				if (remain == s.nUnknown) {
					// The rest are mine
					auto unMe = unknownOf(iter.x(), iter.y());
					for (auto one : unMe) {
						flagMine(one);
						mine.insert(one);
						nchanged++;
					}
					remain = 0;	// This tile is done
				}
			}
			else if (remain == 0) {
				auto unMe = unknownOf(iter.x(), iter.y());
				for (auto one : unMe) {
					flagSafe(one);
					safe.insert(one);
					nchanged++;
				}
			}
		} while (iter.next());
		return nchanged > 0;
	}

	// Phase Two
	bool analysis_ex(std::set<int>& safe, std::set<int>& mine) {
		using namespace gamedata;
		int nchanged = 0;
		auto item = whole_grid();
		do {
			if (item.at(grid) > 0) {
				// This one is an unknown
				// ub(unMutual.mines) = min(|unMutual.mines|, it.remain)
				// lb(ubMutual.mines) = max(0, it.remain - |unIt|)
				// 
				int i = item.x();
				int j = item.y();
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

					auto unIt = unknownOf(i2, j2);
					auto unMutual   = set_intersect(unIt, unMe);
					auto unItExcept = set_diff(unIt, unMutual);
					auto unMeExcept = set_diff(unMe, unMutual);
					

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

						if ((int)unMutual.size() >= itRemain) {
							// And all in unItExcept may be safe
							// Note unMutual.size() should always > itRemain if phase 1 is properly done
							for (auto one : unItExcept) {
								flagSafe(one);
								safe.insert(one);
								nchanged++;
							}
							if ((int)unMutual.size() == itRemain) {
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
						if ((int)unItExcept.size() <= itRemain) {
							// and all in unItExcept may be mines
							for (auto one : unItExcept) {
								flagMine(one);
								mine.insert(one);
								nchanged++;
							}
							if ((int)unItExcept.size() == itRemain) {
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
		} while (item.next());

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

	Stats stats(int i, int j) {
		Stats s;
		GridIter iter(i, j);
		do {
			int type = iter.at(grid);
			if (basicTileType(type) == TileUnknown) {
				if (type == TileSafe)    s.nSafe++;
				if (type == TileMine)    s.nMine++;
				if (type == TileUnknown) s.nUnknown++;
			}
		} while (iter.next());
		return s;
	}

	std::set<int> last_safe;
	std::set<int> last_mine;
	grid_t grid;
};

#include <iostream>
using std::cin;
int main() {
	using namespace gamedata;
	sysapi::auxiliary_init();
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
		}
		if (cmd == "play") {
			mine.reset_grid();
			mine.refresh_rect();
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

				// Click all the safe tiles
				for (auto one : tile_safe) {
					mine.click(one);
				}
			}
		}
	}

	sysapi::auxiliary_exit();
	return 0;
}


int main_1() {
	using namespace gamedata;
	MineS mine;
	sysapi::auxiliary_init();
	sysapi::Rect& rect = mine.rect;
	if (!sysapi::getWindowRect(rect)) {
		puts("Failed to get Window Rect of Arbiter");
	}
	else {
		fmt::print("WinRect: L/R/T/B, {}/{}/{}/{}\n",
			rect.left, rect.right, rect.top, rect.bottom);
	}

	int x, y;
	sysapi::getMousePosition(x, y);
	fmt::print("Mouse: x/y, {}/{}\n", x, y);

	while (1) {
		char c = getchar();
		if (c == 'c') {
			puts("Capturing screen");
			sysapi::captureScreen();
		}
		if (c == 'p') {
			sysapi::getMousePosition(x, y);
			
			int dx = x - rect.left - MineS::base[0];
			int dy = y - rect.top - MineS::base[1];
			int nx = int(dx*1.0 / MineS::dq + 0.5);
			int ny = int(dy*1.0 / MineS::dq + 0.5);
			unsigned int color = mine.color(nx, ny);
			unsigned int color1 = sysapi::getScreenColor(x, y);
			int tile = mine.tile(nx, ny);

			fmt::print("Mouse: x/y, {}/{}, at Tile {}, {}, Color {:x}, color {:x}, status: {}, tile: {}\n", 
				x, y, nx, ny, color1, color, (mine.good()? "good": "bad"), tile);
		}
		if (c == 'm') {
			sysapi::getMousePosition(x, y);

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

	sysapi::auxiliary_exit();
	return 0;
}