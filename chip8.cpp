#include "chip8.hpp"

// debug
#include <string>

Chip8::Chip8()
{
    // init memory, registers, stack
    for(int i = 0; i < MAX_MEMORY; i++) m_Mem[i] = 0x0;
    for(int i = 0; i < MAX_REGISTERS; i++) m_Reg[i] = 0x0;

    m_IReg = 0x0;
    m_DelayReg = 0x0;
    m_SoundReg = 0x0;
    m_PCounter = 0x0;

    // init key state
    m_KeyState = 0x0;

    // clear display
    clearDisplay();

    processInstruction(0xf31a);
}

Chip8::~Chip8()
{

}

bool Chip8::processInstruction(uint16_t inst)
{
    // first nibble is op code
    uint8_t opcode = (inst & 0xf000) >> 12;
    // the rest of the 12-bits can be a value or address
    uint16_t nnn = (inst & 0x0fff);
    // the last nibble
    uint8_t n = (inst & 0x000f);
    // second nibble
    uint8_t x = (inst & 0x0f00) >> 8;
    // third nibble
    uint8_t y = (inst & 0x00f0) >> 4;
    // last byte
    uint8_t kk = (inst & 0x00ff);

    if(true)
    {
        std::cout << std::hex << "processing instruction:" << int(inst) << std::endl;
        std::cout << "opcode: " << int(opcode) << std::endl;
        std::cout << "nnn   : " << int(nnn) << std::endl;
        std::cout << "n     : " << int(n) << std::endl;
        std::cout << "x     : " << int(x) << std::endl;
        std::cout << "y     : " << int(y) << std::endl;
        std::cout << "kk    : " << int(kk) << std::endl;
    }


    if(opcode == 0x0)
    {
        // 00e0 - clear display
        if(inst == 0x00e0) clearDisplay();
        // 00ee - return from subroutine, pop stack
        else if(inst == 0x00ee)
        {
            if(!m_Stack.empty())
            {
                m_PCounter = m_Stack.back();
                m_Stack.pop_back();
            }
        }
    }
    // jump - set program counter to nnn
    else if(opcode == 0x1)
    {
        m_PCounter = nnn;
    }
    // call address - call subroutine at nnn
    // put current pcounter on top of stack, then set pcounter to nnn
    else if(opcode == 0x2)
    {
        m_Stack.push_back(m_PCounter);
        m_PCounter = nnn;
    }

    return true;
}

void Chip8::clearDisplay()
{
    for(int i = 0; i < DISPLAY_HEIGHT; i++)
        for(int n = 0; n < DISPLAY_WIDTH; n++) m_Display[i][n] = false;

}

