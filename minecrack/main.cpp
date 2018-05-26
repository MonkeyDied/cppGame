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
using std::cin;


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
				if (vNew == TileFlag) {
					if (iter.at(grid) != TileMine) {
						// There seems to be some bugs here, ignoring...
						/*
						fmt::print("Adding a hand-chosen mine point at {}/{}, origin type is {}\n", 
							iter.x(), iter.y(), tileChar(iter.at(grid)));
						flagMine(iter.x(), iter.y());
						*/
					}
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
		size_t size_old = safe.size() + mine.size();
		auto iter = whole_grid();
		do {
			int& remain = iter.at(grid);
			if (remain > 0) {
				Stats s = stats(iter.x(), iter.y());
				if (remain == s.nUnknown) {
					// The rest are mine
					auto unMe = unknownOf(iter.x(), iter.y());
					for (auto one : unMe) {
						flagMine(one, &mine);
					}
					remain = 0;	// This tile is done
				}
			}
			else if (remain == 0) {
				auto unMe = unknownOf(iter.x(), iter.y());
				for (auto one : unMe) {
					flagSafe(one, &safe);
				}
			}
		} while (iter.next());
		return safe.size() + mine.size() > size_old;
	}

	// Phase Two
	bool analysis_ex(std::set<int>& safe, std::set<int>& mine) {
		size_t size_old = safe.size() + mine.size();
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
						flagMine(one, &mine);
					}
					continue;
				}

				auto neighbours = find(grid, GridIter(i, j, 2, 2), TileBorder);
				for (auto idx_border : neighbours) {
					int i2, j2;
					from_serial(idx_border, i2, j2);
					if (grid[i2][j2] == 0) {
						for (auto one : unknownOf(i2, j2)) {
							flagSafe(one, &safe);
						}
					}

					auto unIt = unknownOf(i2, j2);
					auto unMutual   = set_intersect(unIt, unMe);
					if (unMutual.empty())
						continue;

					auto unItExcept = set_diff(unIt, unMutual);
					auto unMeExcept = set_diff(unMe, unMutual);

					int itRemain = grid[i2][j2];
					int ub = std::min((int)unMutual.size(), itRemain);
					int lb = std::max(0, itRemain - (int)unItExcept.size());

					if (ub + unMeExcept.size() == meRemain) {
						// All in unMeExcept are mines
						for (auto one : unMeExcept) {
							flagMine(one, &mine);
						}

						if ((int)unMutual.size() >= itRemain) {
							// And all in unItExcept may be safe
							// Note unMutual.size() should always > itRemain if phase 1 is properly done
							for (auto one : unItExcept) {
								flagSafe(one, &safe);
							}
							if ((int)unMutual.size() == itRemain) {
								for (auto one : unMutual) {
									flagMine(one, &mine);
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
							flagSafe(one, &safe);
						}
						if ((int)unItExcept.size() <= itRemain) {
							// and all in unItExcept may be mines
							for (auto one : unItExcept) {
								flagMine(one, &mine);
							}
							if ((int)unItExcept.size() == itRemain) {
								for (auto one : unMutual) {
									flagSafe(one, &safe);
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

		return safe.size() + mine.size() > size_old;
	}

	bool analysis_guess(std::set<int>& safe, std::set<int>& mine, int max_guess = 2) {
		// TODO: add some mechanism to guess mines when you cant do it by deduction



	}
	
	void flagSafe(int index, std::set<int>* store = nullptr) {
		int i, j;
		from_serial(index, i, j);
		grid[i][j] = TileSafe;
		if (store) store->insert(index);
	}

	void flagMine(int i, int j, std::set<int>* store = nullptr) {
		if (basicTileType(grid[i][j]) == TileUnknown) {
			if (grid[i][j] != TileMine) {
				grid[i][j] = TileMine;
				if (store) store->insert(serial_index(i, j));

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
	void flagMine(int index, std::set<int>* store = nullptr) {
		int x, y;
		from_serial(index, x, y);
		flagMine(x, y, store);
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
		if (cmd == "s") {
			mine.refresh_grid();
			auto c = sysapi::getScreenColor(
				mine.rect.left + MineS::smile[0], 
				mine.rect.top + MineS::smile[1]);

			int s = mine.state();
			fmt::print("State color is {:x}, game state is {}\n", c, s);
		}
		if (cmd == "mcolor") {
			int _x, _y;
			sysapi::captureScreen();
			sysapi::getMousePosition(_x, _y);
			auto c = sysapi::getScreenColor(_x, _y);
			int dx, dy;
			dx = _x - mine.rect.left;
			dy = _y - mine.rect.top;
			fmt::print("Mouse at {}/{}, dx/dy is {}/{}, color is {:x}\n", 
				_x, _y, dx, dy, c);
		}
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
			mine.refresh_grid();

			while (mine.state() == GameRunning) {
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
			// It seems that detecting the simile face is not very easy...
			// The color is right, but there seems to be a lag
			wait(500);
			if (mine.state() == GameWin) {
				fmt::print("I win~\n");
			}
			else if (mine.state() == GameLose) {
				fmt::print("I Lose...\n");
			}
			else {
				fmt::print("----Current algo.grid is----\n");
				print_grid(algo.grid);
			}
		}
	}

	sysapi::auxiliary_exit();
	return 0;
}
