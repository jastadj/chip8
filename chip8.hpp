#ifndef CLASS_CHIP8
#define CLASS_CHIP8

#include <cstdlib>
#include <iostream>
#include <vector>

#define MAX_MEMORY 4096
#define MAX_REGISTERS 16
#define MAX_STACK 16

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

class Chip8
{
private:

    // CHIP-8 Memory
    // chip-8 max memory (4096) 0x000-0xfff
    // first 512 bytes (0x000-0x1ff) reserved for interpreter
    uint8_t m_Mem[MAX_MEMORY];


    // CHIP-8 Registers
    // registers 0x0 - 0xf
    // register 0xf should not be used, internal flag register
    uint8_t m_Reg[MAX_REGISTERS];
    // register I generally used to store addresses, usually only lowest 12 bits used
    uint16_t m_IReg;
    // delay register, 60Hz decrement until 0.  non-zero = delay register is active
    uint8_t m_DelayReg;
    // sound register.  60Hz decrement until 0.  non-zero = sound buzzer is active
    uint8_t m_SoundReg;
    // program counter 16-bit register (points to currently executing address)
    uint16_t m_PCounter;

    // stack, stores addresses that interpreter should be returned to when finished
    // chip-8 allows 16 nested subroutines
    std::vector<uint16_t> m_Stack;



    // display, pixels are either on or off.  display is a 64x32 pixel array
    // sprites are always 8-bits width, and up to 15 lines in height
    bool m_Display[DISPLAY_HEIGHT][DISPLAY_WIDTH];

    // keyboard, keypad only has 0-9, a-f keys
    uint16_t m_KeyState;


    // display
    void clearDisplay();

    // instructions
    bool processInstruction(uint16_t inst);

public:
    Chip8();
    ~Chip8();

};
#endif // CLASS_CHIP8
