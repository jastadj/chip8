#include <cstdlib>
#include <sstream>

#include "chip8.hpp"

#include <SFML/Graphics.hpp>

#define DISPLAY_SCALE 8

int main(int argc, char *argv[])
{
    Chip8 chip8;
    unsigned int dwidth = chip8.getDisplayWidth();
    unsigned int dheight = chip8.getDisplayHeight();

    sf::RenderWindow *screen = new sf::RenderWindow(sf::VideoMode(dwidth * DISPLAY_SCALE, dheight * DISPLAY_SCALE, 32), "Chip-8");

    sf::RectangleShape spixel(sf::Vector2f(DISPLAY_SCALE, DISPLAY_SCALE));

    sf::Font font;

    bool quit = false;
    bool (*chdisp)[DISPLAY_WIDTH] = NULL;

    font.loadFromFile("font.ttf");

    if(!chip8.loadRom("maze.rom")) exit(1);

    while(!quit)
    {
        screen->clear();

        sf::Event event;

        while(screen->pollEvent(event))
        {
            if(event.type == sf::Event::Closed) quit = true;
            else if(event.type == sf::Event::KeyPressed)
            {
                if(event.key.code == sf::Keyboard::Escape) quit = true;
            }
        }

        // update
        chip8.executeNextInstruction();


        // draw
        chdisp = chip8.getDisplay();
        for(int i = 0; i < dheight; i++)
        {
            for(int n = 0; n < dwidth; n++)
            {
                if(chdisp[i][n])
                {
                    spixel.setPosition(sf::Vector2f( n*DISPLAY_SCALE, i*DISPLAY_SCALE));
                    screen->draw(spixel);
                }
            }
        }

        // draw debug info
        std::stringstream regss;
        uint8_t *cregs = new uint8_t[MAX_REGISTERS];
        cregs = chip8.getRegisters();
        for(int i = 0; i < MAX_REGISTERS; i++)
        {
            regss << "R" << i << ":" << std::hex << int(cregs[i]) << std::endl;
        }
        sf::Text regtext(regss.str(), font, 12);
        regtext.setColor(sf::Color::Red);
        screen->draw(regtext);

        // update screen
        screen->display();
    }



    return 0;
}
