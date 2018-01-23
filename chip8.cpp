#include "chip8.hpp"
#include <math.h>
#include <time.h>
#include <fstream>
#include <sstream>

#include "font.hpp"

// debug
#include <string>

Chip8::Chip8()
{
    // init random seed
    srand( time(NULL));

    m_Screen = NULL;
    m_RenderInitialized = false;
    m_CPUTickDelayCounter = 0;
    m_LastTickTime = 0;
    m_RunCPU = false;
    m_RunRender = false;
    m_isPaused = false;

    // init memory, registers, stack
    for(int i = 0; i < MAX_MEMORY; i++) m_Mem[i] = 0x0;
    for(int i = 0; i < MAX_REGISTERS; i++) m_Reg[i] = 0x0;

    m_IReg = 0x0;
    m_DelayReg = 0x0;
    m_SoundReg = 0x0;
    m_PCounter = 0x0;

    // init key state
    m_KeyState = 0x0;

    // initial instructions
    // clear screen
    m_Mem[0x00] = 0x00;
    m_Mem[0x01] = 0xe0;
    // jump to address 0x0200
    m_Mem[0x02] = 0x12;
    m_Mem[0x03] = 0x00;


    // store fonts (80 bytes = 16 characters * 5 bytes) in memory
    // store at mem 0x01af to allow for 80 bytes, stopping before 0x0200
    for(int i = 0; i < 80; i++)
    {
        m_Mem[FONT_ADDR + i] = sysfonts[i];
    }


    // init display
    for(int i = 0; i < DISPLAY_HEIGHT; i++)
    {
        for(int n = 0; n < DISPLAY_WIDTH; n++) m_Display[i][n] = false;
    }

    // create threads
    m_CPUThread = new sf::Thread(&Chip8::CPULoop, this);
    m_RenderThread = new sf::Thread(&Chip8::renderLoop, this);
}

Chip8::~Chip8()
{

}

void Chip8::reset()
{
    m_Chip8Mutex.lock();
    m_DelayMutex.lock();

    // init random seed
    srand( time(NULL));

    // reset vars
    m_CPUTickDelayCounter = 0;
    m_LastTickTime = 0;

    m_IReg = 0x0;
    m_DelayReg = 0x0;
    m_SoundReg = 0x0;
    m_PCounter = 0x0;

    // clear registers
    for(int i = 0; i < MAX_REGISTERS; i++) m_Reg[i] = 0x0;

    // init key state
    m_KeyState = 0x0;

    // clear display
    for(int i = 0; i < DISPLAY_HEIGHT; i++)
    {
        for(int n = 0; n < DISPLAY_WIDTH; n++) m_Display[i][n] = false;
    }

    m_DelayMutex.unlock();
    m_Chip8Mutex.unlock();

}

void Chip8::start()
{
    m_CPUThread->launch();
    m_RenderThread->launch();

    std::cout << "Waiting on CPU thread...\n";
    m_CPUThread->wait();
    std::cout << "Waiting on render thread...\n";
    m_RenderThread->wait();
    std::cout << "Shutdown done.\n";
}

void Chip8::shutdown()
{
    std::cout << "Shutting down...\n";
    m_RunCPU = false;
    m_RunRender = false;
}

std::string Chip8::getDisassembledString(Instruction *inst)
{
    std::stringstream dss;

    dss << std::hex << "0x" << inst->addr << " " << inst->opcode << " " << inst->mnemonic << " " << inst->vars;

    return dss.str();
}

Instruction Chip8::disassemble(uint16_t addr)
{
    Instruction dinst;

    std::stringstream varss;

    // store address
    dinst.addr = addr;

    // get opcode
    dinst.opcode = m_Mem[addr] << 8;
    dinst.opcode = dinst.opcode | m_Mem[addr+1];

    // first nibble is op
    dinst.op = (dinst.opcode & 0xf000) >> 12;
    // the rest of the 12-bits can be a value or address
    dinst.nnn = (dinst.opcode & 0x0fff);
    // the last nibble
    dinst.n = (dinst.opcode & 0x000f);
    // second nibble
    dinst.x = (dinst.opcode & 0x0f00) >> 8;
    // third nibble
    dinst.y = (dinst.opcode & 0x00f0) >> 4;
    // last byte
    dinst.kk = (dinst.opcode & 0x00ff);

    if(dinst.op == 0x0)
    {
        // 00e0 - clear display
        if(dinst.opcode == 0x00e0) dinst.mnemonic = "CLS";
        // 00ee - return from subroutine, pop stack
        else if(dinst.opcode == 0x00ee) dinst.mnemonic = "RET";
    }
    // jump - set program counter to nnn
    else if(dinst.op == 0x1)
    {
        dinst.mnemonic = "JP";
        varss << "PC, " << std::hex << "0x" << dinst.nnn;
        dinst.vars = varss.str();
    }
    // call address - call subroutine at nnn
    // put current pcounter on top of stack, then set pcounter to nnn
    else if(dinst.op == 0x2)
    {
        dinst.mnemonic = "CALL";
        varss << "ST, PC, " << std::hex << "0x" << dinst.nnn;
        dinst.vars = varss.str();
    }
    // skip if register x == kk, increment program counter by 2
    else if(dinst.op == 0x3)
    {
        dinst.mnemonic = "SE";
        varss << std::hex << "V" << dinst.x << ", " << "0x" << dinst.kk;
        dinst.vars = varss.str();
    }
    // skip if register x != kk, increment program counter by 2
    else if(dinst.op == 0x4)
    {
        dinst.mnemonic = "SNE";
        varss << std::hex << "V" << dinst.x << ", " << "0x" << dinst.kk;
        dinst.vars = varss.str();
    }
    // skip if register x is equal to register y
    else if(dinst.op == 0x5)
    {
        dinst.mnemonic = "SE";
        varss << std::hex << "V" << dinst.x << " , V" << dinst.y;
        dinst.vars = varss.str();
    }
    // put value of kk into register x
    else if(dinst.op == 0x6)
    {
        dinst.mnemonic = "LD";
        varss << std::hex << "V" << dinst.x << " , 0x" << dinst.kk;
        dinst.vars = varss.str();
    }
    // add kk to register x
    else if(dinst.op == 0x7)
    {
        dinst.mnemonic = "ADD";
        varss << std::hex << "V" << dinst.x << " , 0x" << dinst.kk;
        dinst.vars = varss.str();
    }
    // register operations
    else if(dinst.op == 0x8)
    {
        // EQUAL, stores reg y into reg x
        if(dinst.n == 0x0)
        {
            dinst.mnemonic = "LD";
            varss << std::hex << "V" << dinst.x << " , V" << dinst.y;
            dinst.vars = varss.str();
        }
        // OR, reg x = reg x OR reg y
        else if(dinst.n == 0x1)
        {
            dinst.mnemonic = "OR";
            varss << std::hex << "V" << dinst.x << " , V" << dinst.y;
            dinst.vars = varss.str();
        }
        // AND, reg x = reg x AND reg y
        else if(dinst.n == 0x2)
        {
            dinst.mnemonic = "AND";
            varss << std::hex << "V" << dinst.x << " , V" << dinst.y;
            dinst.vars = varss.str();
        }
        // XOR, reg x = reg x XOR reg y
        else if(dinst.n == 0x3)
        {
            dinst.mnemonic = "XOR";
            varss << std::hex << "V" << dinst.x << " , V" << dinst.y;
            dinst.vars = varss.str();
        }
        // ADD, reg x = reg x + reg y
        else if(dinst.n == 0x4)
        {
            dinst.mnemonic = "ADD";
            varss << std::hex << "V" << dinst.x << " , V" << dinst.y;
            dinst.vars = varss.str();
        }
        // SUB, reg x = vx - vy
        else if(dinst.n == 0x5)
        {
            dinst.mnemonic = "SUB";
            varss << std::hex << "V" << dinst.x << " , V" << dinst.y;
            dinst.vars = varss.str();
        }
        // SHR (shift right), vx = vx / 2
        else if(dinst.n == 0x6)
        {
            dinst.mnemonic = "SHR";
            varss << std::hex << "V" << dinst.x << " {, V" << dinst.y << "}";
            dinst.vars = varss.str();
        }
        // SUBN, reg x = reg y - reg x
        else if(dinst.n == 0x7)
        {
            dinst.mnemonic = "SUBN";
            varss << std::hex << "V" << dinst.x << " , V" << dinst.y;
            dinst.vars = varss.str();

        }
        // SHL (shift left), reg x = reg x * 2
        else if(dinst.n == 0xe)
        {
            dinst.mnemonic = "SHL";
            varss << std::hex << "V" << dinst.x << " {, V" << dinst.y << "}";
            dinst.vars = varss.str();
        }
    }
    else if(dinst.op == 0x9)
    {
        dinst.mnemonic = "SNE";
        varss << std::hex << "V" << dinst.x << " , V" << dinst.y;
        dinst.vars = varss.str();
    }
    // set register I = nnn
    else if(dinst.op == 0xa)
    {
        dinst.mnemonic = "LD";
        varss << std::hex << "I, " <<  "0x" << dinst.nnn;
        dinst.vars = varss.str();
    }
    // JUMP to location nnn + v0
    else if(dinst.op == 0xb)
    {
        dinst.mnemonic = "JP";
        varss << std::hex << "V0, 0x" << dinst.nnn;
        dinst.vars = varss.str();

    }
    // RANDOM 0-255, then AND with kk and store in reg x
    else if(dinst.op == 0xc)
    {
        dinst.mnemonic = "RND";
        varss << std::hex << "V" << dinst.x << " , 0x" << dinst.kk;
        dinst.vars = varss.str();

    }
    // DRAW n-byte height sprite starting at mem location reg I at regx,regy pixels
    else if(dinst.op == 0xd)
    {
        dinst.mnemonic = "DRW";
        varss << std::hex << "V" << dinst.x << " , V" << dinst.y << ", 0x" << dinst.n;
        dinst.vars = varss.str();
    }
    else if(dinst.op == 0xe)
    {
        // skip next instruction if key value in reg x is pressed
        if(dinst.kk == 0x9e)
        {
            dinst.mnemonic = "SKP";
            varss << std::hex << "V" << dinst.x;
            dinst.vars = varss.str();
        }
        // skip next instruction if key value in reg x is not pressed
        else if(dinst.kk == 0xa1)
        {
            dinst.mnemonic = "SKNP";
            varss << std::hex << "V" << dinst.x;
            dinst.vars = varss.str();
        }
    }
    else if(dinst.op == 0xf)
    {
        // reg x = value of delay timer
        if(dinst.kk == 0x07)
        {
            dinst.mnemonic = "LD";
            varss << std::hex << "V" << dinst.x << " , DT";
            dinst.vars = varss.str();
        }
        // wait for key press, then store key press in vx
        else if(dinst.kk == 0x0a)
        {
            dinst.mnemonic = "LD";
            varss << std::hex << "V" << dinst.x << " , K";
            dinst.vars = varss.str();
        }
        // set delay timer to value in reg x
        else if(dinst.kk == 0x15)
        {
            dinst.mnemonic = "LD";
            varss << std::hex << "DT, V" << dinst.x;
            dinst.vars = varss.str();
        }
        // set sound timer to value of reg x
        else if(dinst.kk == 0x18)
        {
            dinst.mnemonic = "LD";
            varss << std::hex << "ST, V" << dinst.x;
            dinst.vars = varss.str();
        }
        // values of reg I and reg x are added and stored in reg i
        else if(dinst.kk == 0x1e)
        {
            dinst.mnemonic = "ADD";
            varss << std::hex << "I, V" << dinst.x;
            dinst.vars = varss.str();
        }
        // font, set I to location of sprite associated with value in reg x
        else if(dinst.kk == 0x29)
        {
            dinst.mnemonic = "LD";
            varss << std::hex << "F, V" << dinst.x;
            dinst.vars = varss.str();
        }
        // store BCD of vx in memory locations of I, I+1, and I+2
        else if(dinst.kk == 0x33)
        {
            dinst.mnemonic = "LD";
            varss << std::hex << "B, V" << dinst.x;
            dinst.vars = varss.str();
        }
        // store register reg 0 through reg x in memory starting at location in reg i
        else if(dinst.kk == 0x55)
        {
            dinst.mnemonic = "LD";
            varss << std::hex << "[I], V" << dinst.x;
            dinst.vars = varss.str();
        }
        // read values from memory starting at location i into registers reg 0 through reg x
        else if(dinst.kk == 0x65)
        {
            dinst.mnemonic = "LD";
            varss << std::hex << "V" << dinst.x << ", [I]";
            dinst.vars = varss.str();
        }
    }

    return dinst;
}

bool Chip8::processInstruction(Instruction inst)
{
    m_Chip8Mutex.lock();
    m_DelayMutex.lock();

    // advance program counter
    m_PCounter += 2;

    if(inst.op == 0x0)
    {
        // 00e0 - clear display
        if(inst.opcode == 0x00e0)
        {
            for(int iy = 0; iy < DISPLAY_HEIGHT; iy++)
                for(int nx = 0; nx < DISPLAY_WIDTH; nx++) m_Display[iy][nx] = false;
        }
        // 00ee - return from subroutine, pop stack
        else if(inst.opcode == 0x00ee)
        {
            if(!m_Stack.empty())
            {
                m_PCounter = m_Stack.back();
                m_Stack.pop_back();
            }
            else m_isPaused = true;
        }
    }
    // jump - set program counter to nnn
    else if(inst.op == 0x1)
    {
        m_PCounter = inst.nnn;
    }
    // call address - call subroutine at nnn
    // put current pcounter on top of stack, then set pcounter to nnn
    else if(inst.op == 0x2)
    {
        m_Stack.push_back(m_PCounter);
        m_PCounter = inst.nnn;
    }
    // skip if register x == kk, increment program counter by 2
    else if(inst.op == 0x3)
    {
        if(m_Reg[inst.x] == inst.kk) m_PCounter += 2;
    }
    // skip if register x != kk, increment program counter by 2
    else if(inst.op == 0x4)
    {
        if(m_Reg[inst.x] != inst.kk) m_PCounter += 2;
    }
    // skip if register x is equal to register y
    else if(inst.op == 0x5)
    {
        if(m_Reg[inst.x] == m_Reg[inst.y]) m_PCounter += 2;
    }
    // put value of kk into register x
    else if(inst.op == 0x6)
    {
        m_Reg[inst.x] = inst.kk;
    }
    // add kk to register x
    else if(inst.op == 0x7)
    {
        m_Reg[inst.x] = m_Reg[inst.x] + inst.kk;
    }
    // register operations
    else if(inst.op == 0x8)
    {
        // EQUAL, stores reg y into reg x
        if(inst.n == 0x0)
        {
            m_Reg[inst.x] = m_Reg[inst.y];
        }
        // OR, reg x = reg x OR reg y
        else if(inst.n == 0x1)
        {
            m_Reg[inst.x] = m_Reg[inst.x] | m_Reg[inst.y];
        }
        // AND, reg x = reg x AND reg y
        else if(inst.n == 0x2)
        {
            m_Reg[inst.x] = m_Reg[inst.x] & m_Reg[inst.y];
        }
        // XOR, reg x = reg x XOR reg y
        else if(inst.n == 0x3)
        {
            m_Reg[inst.x] = m_Reg[inst.x] ^ m_Reg[inst.y];
        }
        // ADD, reg x = reg x + reg y
        else if(inst.n == 0x4)
        {
            unsigned int result = m_Reg[inst.x] + m_Reg[inst.y];

            // if result overflows register
            if(result > 0xff)
            {
                // set result to lower 8 bits
                result = result & 0xff;
                // set carry flag
                m_Reg[0xf] = 0x1;
            }
            else m_Reg[0xf] = 0x0;

            m_Reg[inst.x] = result;
        }
        // SUB, reg x = vx - vy
        else if(inst.n == 0x5)
        {
            // set not borrow flag if reg x > reg y
            if(m_Reg[inst.x] > m_Reg[inst.y]) m_Reg[0xf] = 0x1;
            else m_Reg[0xf] = 0x0;

            m_Reg[inst.x] = m_Reg[inst.x] - m_Reg[inst.y];
        }
        // SHR (shift right), vx = vx / 2
        else if(inst.n == 0x6)
        {
            // if odd number
            if(m_Reg[inst.x] & 0x1) m_Reg[0xf] = 0x1;
            else m_Reg[0xf] = 0x0;

            m_Reg[inst.x] = m_Reg[inst.x] >> 1;
        }
        // SUBN, reg x = reg y - reg x
        else if(inst.n == 0x7)
        {
            // set not borrow flag if reg y > reg x
            if(m_Reg[inst.y] > m_Reg[inst.x]) m_Reg[0xf] = 0x1;
            else m_Reg[0xf] = 0x0;

            m_Reg[inst.x] = m_Reg[inst.y] - m_Reg[inst.x];
        }
        // SHL (shift left), reg x = reg x * 2
        else if(inst.n == 0xe)
        {
            if(0x80 & m_Reg[inst.x]) m_Reg[0xf] = 0x1;
            else m_Reg[0xf] = 0x0;

            m_Reg[inst.x] = m_Reg[inst.x] << 1;
        }
    }
    else if(inst.op == 0x9)
    {
        // skip next instruction if reg x != reg y
        if(inst.n == 0x0)
        {
            if(m_Reg[inst.x] != m_Reg[inst.y]) m_PCounter += 2;
        }
    }
    // set register I = nnn
    else if(inst.op == 0xa)
    {
        m_IReg = inst.nnn;
    }
    // JUMP to location nnn + v0
    else if(inst.op == 0xb)
    {
        m_PCounter = inst.nnn + m_Reg[0x0];
    }
    // RANDOM 0-255, then AND with kk and store in reg x
    else if(inst.op == 0xc)
    {
        m_Reg[inst.x] = (rand()%256)&inst.kk;
    }
    // DRAW n-byte height sprite starting at mem location reg I at regx,regy pixels
    else if(inst.op == 0xd)
    {
        // check bounds of register x and register y
        if(m_Reg[inst.x] >= 0 && m_Reg[inst.y] >= 0 && m_Reg[inst.x] < DISPLAY_WIDTH && m_Reg[inst.y] < DISPLAY_HEIGHT)
        {
            // for each sprite row
            for(int ny = 0; ny < inst.n; ny++)
            {
                // for each pixel in row
                for(int nx = 0; nx < 8; nx++)
                {
                    // current pixel
                    int px = nx + m_Reg[inst.x];
                    int py = ny + m_Reg[inst.y];

                    // check sprite bit for pixel on or off
                    bool pon = (m_Mem[m_IReg + ny] >> (7-nx)) & 0x1;

                    // wrap pixel coordinates
                    // note : for now, not implementing wrapping
                    if(px >= DISPLAY_WIDTH) continue;
                    if(py >= DISPLAY_HEIGHT) continue;

                    // XOR pixel state with display
                    if(pon != m_Display[py][px]) m_Display[py][px] = true;

                }
            }
        }
    }
    else if(inst.op == 0xe)
    {
        // skip next instruction if key value in reg x is pressed
        if(inst.kk == 0x9e)
        {
            if(m_KeyState == m_Reg[inst.x]) m_PCounter += 2;
        }
        // skip next instruction if key value in reg x is not pressed
        else if(inst.kk == 0xa1)
        {
            if(m_KeyState != m_Reg[inst.x]) m_PCounter += 2;
        }
    }
    else if(inst.op == 0xf)
    {
        // reg x = value of delay timer
        if(inst.kk == 0x07)
        {
            m_Reg[inst.x] = m_DelayReg;
        }
        // wait for key press, then store key press in vx
        else if(inst.kk == 0x0a)
        {
            // need to figure out key input loop first
        }
        // set delay timer to value in reg x
        else if(inst.kk == 0x15)
        {
            m_DelayReg = m_Reg[inst.x];
        }
        // set sound timer to value of reg x
        else if(inst.kk == 0x18)
        {
            m_SoundReg = m_Reg[inst.x];
        }
        // values of reg I and reg x are added and stored in reg i
        else if(inst.kk == 0x1e)
        {
            m_IReg += m_IReg + m_Reg[inst.x];
        }
        // font, set I to location of sprite associated with value in reg x
        else if(inst.kk == 0x29)
        {
            if(m_Reg[inst.x] <= 0xf)
            {
                m_IReg = FONT_ADDR + (m_Reg[inst.x]*5);
            }

        }
        // store BCD of vx in memory locations of I, I+1, and I+2
        else if(inst.kk == 0x33)
        {
            // ??
        }
        // store register reg 0 through reg x in memory starting at location in reg i
        else if(inst.kk == 0x55)
        {
            if(m_Reg[inst.x] < MAX_REGISTERS)
            {
                for(int j = 0; j <= m_Reg[inst.x]; j++)  m_Mem[m_IReg + j] = m_Reg[j];
            }

        }
        // read values from memory starting at location i into registers reg 0 through reg x
        else if(inst.kk == 0x65)
        {
            if(m_Reg[inst.x] < MAX_REGISTERS)
            {
                for(int j = 0; j <= m_Reg[inst.x]; j++)  m_Reg[j] = m_Mem[m_IReg + j];
            }
        }
    }

    m_Chip8Mutex.unlock();
    m_DelayMutex.unlock();

    return true;
}

bool Chip8::executeNextInstruction()
{
    if(processInstruction( disassemble(m_PCounter) ) )
    {
        return true;
    }

    return false;
}

bool Chip8::loadRom(std::string filename, uint16_t addr)
{

    std::ifstream ifile;

    ifile.open(filename.c_str(), std::ios::binary);

    if(!ifile.is_open()) return false;

    m_Chip8Mutex.lock();
    while(!ifile.eof())
    {
        unsigned char b;

        b = ifile.get();

        m_Mem[addr] = uint8_t(b);
        addr++;
    }
    m_Chip8Mutex.unlock();

    ifile.close();

    return true;
}

bool Chip8::initRender()
{
    if(m_RenderInitialized) return false;

    // create render window
    m_Screen = new sf::RenderWindow(sf::VideoMode(DISPLAY_WIDTH * DISPLAY_SCALE, DISPLAY_HEIGHT * DISPLAY_SCALE, 32), "Chip-8");

    m_Font.loadFromFile("font.ttf");

    m_RenderInitialized = true;

    return true;
}


void Chip8::CPULoop()
{
    m_RunCPU = true;

    while(m_RunCPU)
    {

        if(m_isPaused) continue;

        // 1 cpu tick
        if(m_CPUClock.getElapsedTime().asMicroseconds() >= 1851.8)
        {
            // process current instruction at program counter
            executeNextInstruction();

            // tick for delay counter
            m_CPUTickDelayCounter++;
            // delay counter 60Hz (540Hz / 60Hz = 9)
            if(m_CPUTickDelayCounter >= 9)
            {
                m_CPUTickDelayCounter = 0;

                m_DelayMutex.lock();
                if(m_DelayReg > 0) m_DelayReg--;
                if(m_SoundReg > 0) m_SoundReg--;
                m_DelayMutex.unlock();

            }

            m_LastTickTime = m_CPUClock.getElapsedTime().asMicroseconds();

            m_CPUClock.restart();
        }
    }

    std::cout << "CPU thread exiting...\n";

}

void Chip8::renderLoop()
{
    initRender();
    m_RunRender = true;

    // create pixel used for "stamping"
    sf::RectangleShape spixel(sf::Vector2f(DISPLAY_SCALE, DISPLAY_SCALE));

    // window title string stream
    std::stringstream titless;

    while(m_RunRender)
    {
        m_Screen->clear();

        sf::Event event;

        while(m_Screen->pollEvent(event))
        {
            if(event.type == sf::Event::Closed) shutdown();
            else if(event.type == sf::Event::KeyPressed)
            {
                if(event.key.code == sf::Keyboard::Escape) shutdown();
                // pause processing
                else if(event.key.code == sf::Keyboard::P) m_isPaused = !m_isPaused;
                // reset machine
                else if(event.key.code == sf::Keyboard::R) reset();
            }
        }

        // update
        // update title bar to show freq
        titless.str(std::string(""));
        titless << "Chip-8 " << int(pow( (m_LastTickTime / 1000000), -1)) << "Hz";
        if(m_isPaused) titless << " - PAUSED";
        m_Screen->setTitle(titless.str());


        // draw
        //m_Chip8Mutex.lock();
        for(int i = 0; i < DISPLAY_HEIGHT; i++)
        {
            for(int n = 0; n < DISPLAY_WIDTH; n++)
            {
                if(m_Display[i][n])
                {
                    spixel.setPosition(sf::Vector2f( n*DISPLAY_SCALE, i*DISPLAY_SCALE));
                    m_Screen->draw(spixel);
                }
            }
        }
        //m_Chip8Mutex.unlock();

        // update screen
        m_Screen->display();
    }

    std::cout << "Render thread exiting...\n";
}


