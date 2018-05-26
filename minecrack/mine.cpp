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