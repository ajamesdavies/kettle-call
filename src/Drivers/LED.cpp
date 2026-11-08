#include "Drivers/LED.h"

#include <Adafruit_NeoPixel.h>

#include "Core/Config.h"
#include "Core/SystemState.h"


namespace
{
    Adafruit_NeoPixel pixel(LED_COUNT, LED_PIN, NEO_GRB + NEO_KHZ800);

    LedState previous = LedState::Error;
}

void LED::begin()
{
    pixel.begin();
    pixel.setBrightness(25);
}

void LED::update()
{
    if(previous == System.ledState)
        return;

    previous = System.ledState;

    switch(System.ledState)
    {
        case LedState::Boot:
            pixel.setPixelColor(0,pixel.Color(255,150,0));
            break;

        case LedState::Searching:
            pixel.setPixelColor(0,pixel.Color(255,255,0));
            break;

        case LedState::Ready:
            pixel.setPixelColor(0,pixel.Color(0,255,0));
            break;

        case LedState::TX:
            pixel.setPixelColor(0,pixel.Color(255,0,0));
            break;

        case LedState::RX:
            pixel.setPixelColor(0,pixel.Color(0,0,255));
            break;

        default:
            pixel.setPixelColor(0,pixel.Color(255,0,255));
            break;
    }

    pixel.show();
}