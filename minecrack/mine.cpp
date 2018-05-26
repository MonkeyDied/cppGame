#include "mine.hpp"
#include "sysapi.hpp"

char tileChar(int type) {
	char c;
	switch (type) {
	case TileError:   c = '!'; break;
	case TileBoom:    c = '*'; break;
	case TileUnknown: c = '-'; break;
	case TileZero:    c = ' '; break;
	case TileMine:    c = 'm'; break;
	case TileFlag:    c = 'M'; break;
	case TileSafe:    c = 'o'; break;
	default:
		c = '0' + type;
		break;
	}
	return c;
}

void print_grid(grid_t& grid) {
	using namespace gamedata;
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

int MineS::tile(int i, int j) const {
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
		if (c == c_mine) {
			auto c2 = sysapi::getScreenColor(toX(i), toY(j) - 2);
			if (c2 == 0xff) {
				return TileFlag;
			}
			return TileBoom;
		}
		return it - item.begin();
	}
}

const int MineS::base[2] = { 47 - 24, 201 - 92 };
const int MineS::smile[4] = { 255, 75, 257, 73 };
const std::vector<unsigned int> MineS::item = {
	0xc0c0c0,
	0xff0000,
	0x8000,
	0xff,
	0x800000,
	0x80,
	0x808000,
	0x77,	// This is non-exisiting, it's for num 7, whose value is 0
	0		// This is when clicking a mine
};