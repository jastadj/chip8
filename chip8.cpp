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

    // set first instruction to jump to address 0x200
    m_Mem[0x0] = 0x12;
    m_Mem[0x1] = 0x00;
    m_Mem[0x200] = 0x10;
    m_Mem[0x201] = 0x00;

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

bool Chip8::processInstruction(uint16_t inst)
{
    m_Chip8Mutex.lock();
    m_DelayMutex.lock();


    std::string iname = "ERR";

    // original program counter
    uint16_t op = m_PCounter;

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

    // advance program counter
    if(m_PCounter == op) m_PCounter += 2;

    if(opcode == 0x0)
    {
        // 00e0 - clear display
        if(inst == 0x00e0)
        {
            iname = "CLS";

            for(int iy = 0; iy < DISPLAY_HEIGHT; iy++)
                for(int nx = 0; nx < DISPLAY_WIDTH; nx++) m_Display[iy][nx] = false;
        }
        // 00ee - return from subroutine, pop stack
        else if(inst == 0x00ee)
        {
            if(!m_Stack.empty())
            {
                iname = "RET";

                m_PCounter = m_Stack.back();
                m_Stack.pop_back();
            }
            else shutdown();
        }
    }
    // jump - set program counter to nnn
    else if(opcode == 0x1)
    {
        iname = "JP";

        m_PCounter = nnn;
    }
    // call address - call subroutine at nnn
    // put current pcounter on top of stack, then set pcounter to nnn
    else if(opcode == 0x2)
    {
        iname = "CALL";

        m_Stack.push_back(m_PCounter);
        m_PCounter = nnn;
    }
    // skip if register x == kk, increment program counter by 2
    else if(opcode == 0x3)
    {
        iname = "SE";

        if(m_Reg[x] == kk) m_PCounter += 2;
    }
    // skip if register x != kk, increment program counter by 2
    else if(opcode == 0x4)
    {
        iname = "SNE";

        if(m_Reg[x] != kk) m_PCounter += 2;
    }
    // skip if register x is equal to register y
    else if(opcode == 0x5)
    {
        iname = "SEV";

        if(m_Reg[x] == m_Reg[y]) m_PCounter += 2;
    }
    // put value of kk into register x
    else if(opcode == 0x6)
    {
        iname = "LD";

        m_Reg[x] = kk;
    }
    // add kk to register x
    else if(opcode == 0x7)
    {
        iname = "ADD";

        m_Reg[x] = m_Reg[x] + kk;
    }
    // register operations
    else if(opcode == 0x8)
    {
        // EQUAL, stores reg y into reg x
        if(n == 0x0)
        {
            iname = "LDV";

            m_Reg[x] = m_Reg[y];
        }
        // OR, reg x = reg x OR reg y
        else if(n == 0x1)
        {
            iname = "OR";

            m_Reg[x] = m_Reg[x] | m_Reg[y];
        }
        // AND, reg x = reg x AND reg y
        else if(n == 0x2)
        {
            iname = "AND";

            m_Reg[x] = m_Reg[x] & m_Reg[y];
        }
        // XOR, reg x = reg x XOR reg y
        else if(n == 0x3)
        {
            iname = "XOR";

            m_Reg[x] = m_Reg[x] ^ m_Reg[y];
        }
        // ADD, reg x = reg x + reg y
        else if(n == 0x4)
        {
            unsigned int result = m_Reg[x] + m_Reg[y];

            iname = "ADDV";

            // if result overflows register
            if(result > 0xff)
            {
                // set result to lower 8 bits
                result = result & 0xff;
                // set carry flag
                m_Reg[0xf] = 0x1;
            }
            else m_Reg[0xf] = 0x0;

            m_Reg[x] = result;
        }
        // SUB, reg x = vx - vy
        else if(n == 0x5)
        {
            iname = "SUB";

            // set not borrow flag if reg x > reg y
            if(m_Reg[x] > m_Reg[y]) m_Reg[0xf] = 0x1;
            else m_Reg[0xf] = 0x0;

            m_Reg[x] = m_Reg[x] - m_Reg[y];
        }
        // SHR (shift right), vx = vx / 2
        else if(n == 0x6)
        {
            iname = "SHR";

            // if odd number
            if(m_Reg[x] & 0x1) m_Reg[0xf] = 0x1;
            else m_Reg[0xf] = 0x0;

            m_Reg[x] = m_Reg[x] >> 1;
        }
        // SUBN, reg x = reg y - reg x
        else if(n == 0x7)
        {
            iname = "SUB";

            // set not borrow flag if reg y > reg x
            if(m_Reg[y] > m_Reg[x]) m_Reg[0xf] = 0x1;
            else m_Reg[0xf] = 0x0;

            m_Reg[x] = m_Reg[y] - m_Reg[x];
        }
        // SHL (shift left), reg x = reg x * 2
        else if(n == 0xe)
        {
            iname = "SHL";

            if(0x80 & m_Reg[x]) m_Reg[0xf] = 0x1;
            else m_Reg[0xf] = 0x0;

            m_Reg[x] = m_Reg[x] << 1;
        }
    }
    else if(opcode == 0x9)
    {
        iname = "SNE";

        // skip next instruction if reg x != reg y
        if(n == 0x0)
        {
            if(m_Reg[x] != m_Reg[y]) m_PCounter += 2;
        }
    }
    // set register I = nnn
    else if(opcode == 0xa)
    {
        iname = "LD";

        m_IReg = nnn;
    }
    // JUMP to location nnn + v0
    else if(opcode == 0xb)
    {
        iname = "JP";

        m_PCounter = nnn + m_Reg[0x0];
    }
    // RANDOM 0-255, then AND with kk and store in reg x
    else if(opcode == 0xc)
    {
        iname = "RND";
        m_Reg[x] = (rand()%256)&kk;
    }
    // DRAW n-byte height sprite starting at mem location reg I at regx,regy pixels
    else if(opcode == 0xd)
    {
        // check bounds of register x and register y
        if(m_Reg[x] >= 0 && m_Reg[y] >= 0 && m_Reg[x] < DISPLAY_WIDTH && m_Reg[y] < DISPLAY_HEIGHT)
        {
            iname = "DRW";

            // for each sprite row
            for(int ny = 0; ny < n; ny++)
            {
                // for each pixel in row
                for(int nx = 0; nx < 8; nx++)
                {
                    // current pixel
                    int px = nx + m_Reg[x];
                    int py = ny + m_Reg[y];

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
    else if(opcode == 0xe)
    {
        // skip next instruction if key value in reg x is pressed
        if(kk == 0x9e)
        {
            iname = "SKP";

            if(m_KeyState == m_Reg[x]) m_PCounter += 2;
        }
        // skip next instruction if key value in reg x is not pressed
        else if(kk == 0xa1)
        {
            iname = "SKP";

            if(m_KeyState != m_Reg[x]) m_PCounter += 2;
        }
    }
    else if(opcode == 0xf)
    {
        // reg x = value of delay timer
        if(kk == 0x07)
        {
            iname = "LD";

            m_Reg[x] = m_DelayReg;
        }
        // wait for key press, then store key press in vx
        else if(kk == 0x0a)
        {
            iname = "LD";

            // need to figure out key input loop first
        }
        // set delay timer to value in reg x
        else if(kk == 0x15)
        {
            iname = "LD";

            m_DelayReg = m_Reg[x];
        }
        // set sound timer to value of reg x
        else if(kk == 0x18)
        {
            iname = "LD";

            m_SoundReg = m_Reg[x];
        }
        // values of reg I and reg x are added and stored in reg i
        else if(kk == 0x1e)
        {
            iname = "ADD";

            m_IReg += m_IReg + m_Reg[x];
        }
        // font, set I to location of sprite associated with value in reg x
        else if(kk == 0x29)
        {
            if(m_Reg[x] <= 0xf)
            {
                iname = "LD";
                m_IReg = FONT_ADDR + (m_Reg[x]*5);
            }

        }
        // store BCD of vx in memory locations of I, I+1, and I+2
        else if(kk == 0x33)
        {
            iname = "LD";

            // ??
        }
        // store register reg 0 through reg x in memory starting at location in reg i
        else if(kk == 0x55)
        {
            if(m_Reg[x] < MAX_REGISTERS)
            {
                iname = "LD";

                for(int j = 0; j <= m_Reg[x]; j++)  m_Mem[m_IReg + j] = m_Reg[j];
            }

        }
        // read values from memory starting at location i into registers reg 0 through reg x
        else if(kk == 0x65)
        {
            if(m_Reg[x] < MAX_REGISTERS)
            {
                iname = "LD";

                for(int j = 0; j <= m_Reg[x]; j++)  m_Reg[j] = m_Mem[m_IReg + j];
            }
        }
    }



    if(false)
    {
        std::cout << std::hex << "processing instruction:" << int(inst) << std::endl;
        std::cout << "opcode: " << int(opcode) << std::endl;
        std::cout << "nnn   : " << int(nnn) << std::endl;
        std::cout << "n     : " << int(n) << std::endl;
        std::cout << "x     : " << int(x) << std::endl;
        std::cout << "y     : " << int(y) << std::endl;
        std::cout << "kk    : " << int(kk) << std::endl;
        std::cout << std::hex << m_PCounter << " : " << iname << std::endl;
    }



    m_Chip8Mutex.unlock();
    m_DelayMutex.unlock();

    return true;
}

bool Chip8::executeNextInstruction()
{
    if(processInstruction( (m_Mem[m_PCounter] << 8) | (m_Mem[m_PCounter+1])     ) )
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
                else if(event.key.code == sf::Keyboard::P) m_isPaused = !m_isPaused;
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


