#ifndef CLASS_CHIP8
#define CLASS_CHIP8

#include <cstdlib>
#include <iostream>
#include <vector>

#include <SFML/Graphics.hpp>

#define MAX_MEMORY 4096
#define MAX_REGISTERS 16
#define MAX_STACK 16

#define DISPLAY_WIDTH 64
#define DISPLAY_HEIGHT 32

#define FONT_ADDR 0x1af

#define DISPLAY_SCALE 8

struct Instruction
{
    // short description of instruction
    std::string mnemonic;
    // what the instruction affects (registers, etc)
    std::string vars;
    // store address of instruction
    uint16_t addr;
    // original program counter
    uint16_t opcode;
    // first nibble is operation
    uint8_t op;
    // the rest of the 12-bits can be a value or address
    uint16_t nnn;
    // the last nibble
    uint8_t n;
    // second nibble
    uint8_t x;
    // third nibble
    uint8_t y;
    // last byte
    uint8_t kk;
};

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
    uint8_t m_KeyState;

    // thread control
    sf::Thread *m_CPUThread;
    sf::Thread *m_RenderThread;
    sf::Mutex m_Chip8Mutex;
    sf::Mutex m_DelayMutex;
    bool m_RunCPU;
    bool m_RunRender;


    // processing
    sf::Clock m_CPUClock;
    int m_CPUTickDelayCounter;
    double m_LastTickTime;
    bool m_isPaused;
    bool processInstruction(Instruction inst);
    bool executeNextInstruction();
    Instruction disassemble(uint16_t addr);
    void CPULoop();
    void reset();

    // SFML Rendering
    bool initRender();
    bool m_RenderInitialized;
    sf::RenderWindow *m_Screen;
    sf::Font m_Font;
    void renderLoop();
    void drawDebug();

    std::string getDisassembledString(Instruction *inst);

public:
    Chip8();
    ~Chip8();

    typedef bool (*pointer_to_display)[DISPLAY_WIDTH];

    // get display
    unsigned int getDisplayWidth() { return DISPLAY_WIDTH;}
    unsigned int getDisplayHeight() { return DISPLAY_HEIGHT;}
    pointer_to_display getDisplay() { return m_Display;}

    // get memory
    uint16_t getProgramCounter() { return m_PCounter;}
    uint8_t getMemAt(uint16_t addr) { return m_Mem[addr];}

    // get registers
    uint8_t *getRegisters() { return m_Reg;}
    uint16_t getIRegister() { return m_IReg;}
    uint8_t getDelayRegister() { return m_DelayReg;}
    uint8_t getSoundRegister() { return m_SoundReg;}

    // get stack
    std::vector<uint16_t> getStack() { return m_Stack;}

    bool loadRom(std::string filename, uint16_t addr = 0x200);
    void start();
    void shutdown();
    void pause(bool npause) {m_isPaused = npause;}
    bool isPaused() { return m_isPaused;}
};
#endif // CLASS_CHIP8
