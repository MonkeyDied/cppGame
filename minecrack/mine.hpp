#pragma once
/*
 * A Minesweeper client interface
 */
 
#include "sysapi.hpp"
#include "fmt/format.h"
#include <array>
#include <thread>
#include <chrono>
#include <set>

// Utility Functions
namespace gamedata {
	const int nX = 30;
	const int nY = 16;
}

using grid_t = std::array<std::array<int, gamedata::nY>, gamedata::nX>;
inline int serial_index(int x, int y) { return y*gamedata::nX + x; }
inline void from_serial(int s, int& x, int& y) {
	x = s % gamedata::nX;
	y = s / gamedata::nX;
}
inline void wait(unsigned int ms) {
	std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

// Tile Info
enum {
	TileError = -3,
	TileBoom = -2,
	TileUnknown = -1,
	TileMine = -4,	// An Unknown tile deduced to be mine
	TileSafe = -5,	// An Unknown tile deduced to be safe
	TileBorderWithZero = -7,	// For tile selection
	TileBorder = -8,			// For tile selection	
	TileZero = 0
};
inline int basicTileType(int type) {
	if (type == TileMine || type == TileSafe)
		return TileUnknown;
	return type;
}
char tileChar(int type);
void print_grid(grid_t& grid);


// A iterator used to iterate a neighbourhood around a point
struct GridIter {
	GridIter(int cX = 0, int cY = 0, int rX = 1, int rY = 1) :
		_x(cX), _y(cY), _ni(rX), _nj(rY)
	{
		reset();
	}

	int& at(grid_t& grid) { return grid[x()][y()]; }
	bool valid() { return !(i > _ni || x() >= gamedata::nX); }
	
	int x() const { return _x + i; }
	int y() const { return _y + j; }
	void reset() {
		i = -_ni;
		j = -_nj;
		while (x() < 0) ++i;
		while (y() < 0) ++j;
	}
	
	bool next() {
		if (!valid()) return false;

		j = j + 1;
		if (j > _nj || y() >= gamedata::nY) {
			j = -_nj;
			while (y() < 0) ++j;
			i = i + 1;
		}
		return valid();
	}

	int _x, _y, i, j;
	int _ni, _nj;
};

inline GridIter whole_grid() {
	using namespace gamedata;
	int x = nX / 2;
	int y = nY / 2;
	int rx = std::max(x, nX - x - 1);
	int ry = std::max(y, nY - y - 1);
	return GridIter(x, y, rx, ry);
}

inline std::set<int> find(grid_t& grid, GridIter it, int tile_type) {
	std::set<int> ret;
	do {
		int one = serial_index(it.x(), it.y());
		if (tile_type == TileBorderWithZero || tile_type == TileBorder) {
			if (it.at(grid) > 0) {
				ret.insert(one);
			}
			if (tile_type == TileBorder && it.at(grid) == 0) {
				ret.insert(one);
			}
		}
		else if (it.at(grid) == tile_type) {
			ret.insert(one);
		}

	} while (it.next());
	return ret;
}





// Minesweeper client
class MineS {
public:
	void sim_click(int i, int j) const {
		sysapi::mouseMoveTo(toX(i), toY(j));
		sysapi::mouseLeftClick();
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
		unsigned int c = sysapi::getScreenColor(toX(i), toY(j));
		if (c == item[0]) {
			// need further check
			unsigned int c2 = sysapi::getScreenColor(toX(i), toY(j) - 7);
			//fmt::print("Color below 8 {:x}\n", c2);
			if (c2 == 0xffffff){
				return TileUnknown;
			}
			c2 = sysapi::getScreenColor(toX(i) + 1, toY(j));
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
		return sysapi::getScreenColor(toX(i), toY(j));
	}

	bool good() const {
		unsigned int c = sysapi::getScreenColor(rect.left + smile[0], rect.top + smile[1]);
		if (c == 0xffff) 
			return true;
		return false;
	}
	void reset() {
		sysapi::mouseMoveTo(rect.left + smile[0], rect.top + smile[1]);
		sysapi::mouseLeftClick();
		reset_grid();
		wait(50);
	}
	void reset_grid() {
		for (auto& one : grid) {
			one.fill(TileUnknown);
		}
	}
	bool refresh_rect() {
		return sysapi::getWindowRect(rect);
	}
	void refresh_grid() {
		using namespace gamedata;
		sysapi::captureScreen();
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

	sysapi::Rect rect;
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