#include <cstdlib>
#include <sstream>

#include "chip8.hpp"



int main(int argc, char *argv[])
{
    Chip8 chip8;
    chip8.disassembleRomToASM("pong.rom", "pong.asm");
    chip8.disassembleRomToASM("pong.rom", "pong_verbose.asm", true);
    //chip8.loadRom("pong.rom");
    //chip8.disableRender();
    //chip8.start();

    return 0;
}
