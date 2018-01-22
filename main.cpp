#include <cstdlib>
#include <sstream>

#include "chip8.hpp"



int main(int argc, char *argv[])
{
    Chip8 chip8;

    chip8.loadRom("maze.rom");
    chip8.start();

    return 0;
}
