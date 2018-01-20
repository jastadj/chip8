#include <cstdlib>

#include "chip8.hpp"

#include <SFML/Graphics.hpp>

#define DISPLAY_SCALE 8

int main(int argc, char *argv[])
{
    Chip8 chip8;

    sf::RenderWindow *screen = new sf::RenderWindow(sf::VideoMode(chip8.getDisplayWidth() * DISPLAY_SCALE, chip8.getDisplayHeight() * DISPLAY_SCALE, 32), "Chip-8");

    sf::RectangleShape spixel(sf::Vector2f(DISPLAY_SCALE, DISPLAY_SCALE));

    bool quit = false;


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

        // draw

        // update screen
        screen->display();
    }



    return 0;
}
