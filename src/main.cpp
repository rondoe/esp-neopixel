#include "Arduino.h"
#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <ESP8266httpUpdate.h>

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager
#include <PubSubClient.h>

#include <EEPROM.h>

WiFiClient espClient;
PubSubClient client(espClient);

#include <FastLED.h>


byte brightness = 50;
// How many leds in your strip?
int numberLeds = 300;
#define DATA_PIN 4
#define FRAMES_PER_SECOND  120
#define MAX_BRIGHTNESS 50
#define MAX_LEDS          300
// Define the array of leds => 60 leds/m => 15m
CRGB leds[MAX_LEDS];

byte red, green, blue;

// animation flag
byte animation = 0;

void checkUpdate() {
        Serial.println("Check for updates");
        t_httpUpdate_return ret = ESPhttpUpdate.update("https://upthrow.rondoe.com/update/esp?token=xhestcal842myu1ewrupfgpxsy1b352cq2psfohs2nrdxmvsxu0036pw9tdh5f4b", "", "A8 A8 4C 5D C7 5D 03 BB 1A 4C 47 13 B9 33 4C A6 4D AF 4A 63");

        switch(ret) {
        case HTTP_UPDATE_FAILED:
                Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s", ESPhttpUpdate.getLastError(), ESPhttpUpdate.getLastErrorString().c_str());
                Serial.println();
                break;

        case HTTP_UPDATE_NO_UPDATES:
                Serial.println("HTTP_UPDATE_NO_UPDATES");
                break;

        case HTTP_UPDATE_OK:
                Serial.println("HTTP_UPDATE_OK");
                break;
        }
}

void setColor(byte r, byte g, byte b) {

        for (int i = 0; i < numberLeds; i++) {
                leds[i].r = r;
                leds[i].g = g;
                leds[i].b = b;
        }

        red = r;
        green = g;
        blue = b;

        EEPROM.write(2, r);
        EEPROM.write(3, g);
        EEPROM.write(4, b);
        EEPROM.commit();

        animation = 0;

}
void setBrightness(byte dim) {
        // Return if 0, user must turn off
        if(dim ==0 ) {
                return;
        }

        // maximum is 200
        if(dim > 200) {
                dim = 200;
        }


        FastLED.setBrightness( dim);
        brightness = dim;
        // write brightness to eeprom
        EEPROM.write(1, dim);
        EEPROM.commit();
}

void off() {
        setColor(0, 0, 0);
        FastLED.setBrightness( 0);
}



void on() {
        Serial.println("Turn on");
        // warm white
        // setColor(255, 147, 41);
        setColor(red, green, blue);
        setBrightness(brightness);
}


void stripSetup() {

        // first reset all to black
        FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, MAX_LEDS);
        for (int i = 0; i < MAX_LEDS; i++) {
                leds[i].r = 0;
                leds[i].g = 0;
                leds[i].b = 0;
        }
        FastLED.show(); // display this frame
        // reset the fastled
        FastLED.clear(true);

        FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, numberLeds);
}

void wifi() {
        //WiFiManager
        //Local intialization. Once its business is done, there is no need to keep it around
        WiFiManager wifiManager;
        wifiManager.setConfigPortalTimeout(180);
        if (!wifiManager.autoConnect("AutoConnectAP", "password")) {
                Serial.println("failed to connect, we should reset as see if it connects");
                delay(3000);
                ESP.reset();
                delay(5000);
        }

        //if you get here you have connected to the WiFi
        Serial.println("connected...yeey :)");


        Serial.println("local ip");
        Serial.println(WiFi.localIP());
}

void callback(char* topic, byte* payload, unsigned int length) {
        Serial.print("Message arrived [");
        Serial.print(topic);
        Serial.print("] ");
        String msg="";
        for (int i = 0; i < length; i++) {
                // Serial.print((char)payload[i]);
                msg+=(char)payload[i];
        }
        Serial.println(msg);
        String t = String(topic);
        if (t.equals("myhome/200/0/V_DIMMER")) {
                Serial.println("Set bright");
                setBrightness(msg.toInt());
                return;
        }

        if (t.equals("myhome/200/0/V_RGB")) {
                byte pos = 0;
                if(msg.indexOf('#') == 0) return;
                // Get rid of '#' and convert it to integer
                char subbuff[6];
                long number = (long) strtol(msg.c_str(), NULL, 16);
                // Split them up into r, g, b values
                byte r = number >> 16;
                byte g = number >> 8 & 0xFF;
                byte b = number & 0xFF;
                setColor(r, g, b);
        }

        if (t.equals("myhome/200/0/V_ANIMATION")) {
                animation = msg.toInt();
                if(animation == 0) {
                        setColor(red, green, blue);
                }
        }

        if(t.equals("myhome/200/0/V_UPDATE")) {
                checkUpdate();
        }
}

void reconnect() {
        // Loop until we're reconnected
        while (!client.connected()) {
                Serial.print("Attempting MQTT connection...");
                // Create a random client ID
                String clientId = "ESP8266LED-";
                clientId += String(random(0xffff), HEX);
                // Attempt to connect
                if (client.connect(clientId.c_str())) {
                        Serial.println("connected");
                        // Once connected, publish an announcement...
                        // client.publish("outTopic", "hello world");
                        // ... and resubscribe
                        client.subscribe("myhome/200/0/V_RGB");
                        client.subscribe("myhome/200/0/V_DIMMER");
                        client.subscribe("myhome/200/0/V_ANIMATION");
                        client.subscribe("myhome/200/0/V_UPDATE");
                } else {
                        Serial.print("failed, rc=");
                        Serial.print(client.state());
                        Serial.println(" try again in 5 seconds");
                        // Wait 5 seconds before retrying
                        delay(5000);
                }
        }
}

void initEEPROM() {
        // read eeprom for values
        EEPROM.begin(512);
        byte initialized = EEPROM.read(0);
        if(!initialized) {
                EEPROM.write(1, 20);
                EEPROM.write(2, 255);
                EEPROM.write(3, 244);
                EEPROM.write(4, 229);

                // set to initialized
                EEPROM.write(0, 1);
                EEPROM.commit();
        }

        brightness = EEPROM.read(1);
        red = EEPROM.read(2);
        green = EEPROM.read(3);
        blue = EEPROM.read(4);
}


/////// ANIMATIONS


void blur() {
        uint8_t blurAmount = dim8_raw( beatsin8(3,64, 192) ); // A sinewave at 3 Hz with values ranging from 64 to 192.
        blur1d( leds, numberLeds, blurAmount);                // Apply some blurring to whatever's already on the strip, which will eventually go black.

        uint8_t i = beatsin8(  9, 0, numberLeds);
        uint8_t j = beatsin8( 7, 0, numberLeds);
        uint8_t k = beatsin8(  5, 0, numberLeds);

// The color of each point shifts over time, each at a different speed.
        uint16_t ms = millis();
        leds[(i+j)/2] = CHSV( ms / 29, 200, 255);
        leds[(j+k)/2] = CHSV( ms / 41, 200, 255);
        leds[(k+i)/2] = CHSV( ms / 73, 200, 255);
        leds[(k+i+j)/3] = CHSV( ms / 53, 200, 255);
}


// Palette definitions
CRGBPalette16 currentPalette = RainbowColors_p;
CRGBPalette16 targetPalette;
TBlendType currentBlending = LINEARBLEND;



void beatwave() {

        uint8_t wave1 = beatsin8(9, 0, 255);                  // That's the same as beatsin8(9);
        uint8_t wave2 = beatsin8(8, 0, 255);
        uint8_t wave3 = beatsin8(7, 0, 255);
        uint8_t wave4 = beatsin8(6, 0, 255);

        for (int i=0; i<numberLeds; i++) {
                leds[i] = ColorFromPalette( currentPalette, i+wave1+wave2+wave3+wave4, 255, currentBlending);
        }

} // beatwave()


void blendme() {
        uint8_t starthue = beatsin8(20, 0, 255);
        uint8_t endhue = beatsin8(35, 0, 255);
        if (starthue < endhue) {
                fill_gradient(leds, numberLeds, CHSV(starthue,255,255), CHSV(endhue,255,255), FORWARD_HUES); // If we don't have this, the colour fill will flip around
        } else {
                fill_gradient(leds, numberLeds, CHSV(starthue,255,255), CHSV(endhue,255,255), BACKWARD_HUES);
        }
} // blendme()

// Define variables used by the sequences.
uint8_t thisdelay =   10;                                         // A delay value for the sequence(s)
uint8_t count =   0;                                          // Count up to 255 and then reverts to 0
uint8_t fadeval = 224;                                        // Trail behind the LED's. Lower => faster fade.

uint8_t bpm = 30;

void dot_beat() {

        uint8_t inner = beatsin8(bpm, numberLeds/4, numberLeds/4*3); // Move 1/4 to 3/4
        uint8_t outer = beatsin8(bpm, 0, numberLeds-1);         // Move entire length
        uint8_t middle = beatsin8(bpm, numberLeds/3, numberLeds/3*2); // Move 1/3 to 2/3

        leds[middle] = CRGB::Purple;
        leds[inner] = CRGB::Blue;
        leds[outer] = CRGB::Aqua;

        nscale8(leds,numberLeds,fadeval);                       // Fade the entire array. Or for just a few LED's, use  nscale8(&leds[2], 5, fadeval);

} // dot_beat()



void ease() {

        static uint8_t easeOutVal = 0;
        static uint8_t easeInVal  = 0;
        static uint8_t lerpVal    = 0;

        easeOutVal = ease8InOutQuad(easeInVal);               // Start with easeInVal at 0 and then go to 255 for the full easing.
        easeInVal++;

        lerpVal = lerp8by8(0, numberLeds, easeOutVal);          // Map it to the number of LED's you have.

        leds[lerpVal] = CRGB::Red;
        fadeToBlackBy(leds, numberLeds, 16);                    // 8 bit, 1 = slow fade, 255 = fast fade

} // ease()


// Define variables used by the sequence.

uint8_t thisfade = 192;                                       // How quickly does it fade? Lower = slower fade rate.

void mover() {
        static uint8_t hue = 0;
        for (int i = 0; i < numberLeds; i++) {
                leds[i] += CHSV(hue, 255, 255);
                leds[(i+5) % numberLeds] += CHSV(hue+85, 255, 255); // We use modulus so that the location is between 0 and NUM_LEDS
                leds[(i+10) % numberLeds] += CHSV(hue+170, 255, 255); // Same here.
                show_at_max_brightness_for_power();
                fadeToBlackBy(leds, numberLeds, thisfade);      // Low values = slower fade.
                delay(thisdelay);                             // UGH!!!! A blocking delay. If you want to add controls, they may not work reliably.
        }
} // mover()


void ChangeMe() {                                             // A time (rather than loop) based demo sequencer. This gives us full control over the length of each sequence.
        uint8_t secondHand = (millis() / 1000) % 15;          // IMPORTANT!!! Change '15' to a different value to change duration of the loop.
        static uint8_t lastSecond = 99;                       // Static variable, means it's only defined once. This is our 'debounce' variable.
        if (lastSecond != secondHand) {                       // Debounce to make sure we're not repeating an assignment.
                lastSecond = secondHand;
                switch(secondHand) {
                case  0: thisdelay=20; thisfade=240; break;   // You can change values here, one at a time , or altogether.
                case  5: thisdelay=50; thisfade=128; break;
                case 10: thisdelay=100; thisfade=64; break;   // Only gets called once, and not continuously for the next several seconds. Therefore, no rainbows.
                case 15: break;
                }
        }
} // ChangeMe()


uint8_t maxChanges = 24;

void noise16_1() {                                            // moves a noise up and down while slowly shifting to the side

        uint16_t scale = 1000;                                // the "zoom factor" for the noise

        for (uint16_t i = 0; i < numberLeds; i++) {

                uint16_t shift_x = beatsin8(5);               // the x position of the noise field swings @ 17 bpm
                uint16_t shift_y = millis() / 100;            // the y position becomes slowly incremented


                uint16_t real_x = (i + shift_x)*scale;        // the x position of the noise field swings @ 17 bpm
                uint16_t real_y = (i + shift_y)*scale;        // the y position becomes slowly incremented
                uint32_t real_z = millis() * 20;              // the z position becomes quickly incremented

                uint8_t noise = inoise16(real_x, real_y, real_z) >> 8; // get the noise data and scale it down

                uint8_t index = sin8(noise*3);               // map LED color based on noise data
                uint8_t bri   = noise;

                leds[i] = ColorFromPalette(currentPalette, index, bri, LINEARBLEND); // With that value, look up the 8 bit colour palette value and assign it to the current LED.
        }

} // noise16_1()


void noise16_2() {                                            // just moving along one axis = "lavalamp effect"

        uint8_t scale = 1000;                                 // the "zoom factor" for the noise

        for (uint16_t i = 0; i < numberLeds; i++) {

                uint16_t shift_x = millis() / 10;             // x as a function of time
                uint16_t shift_y = 0;

                uint32_t real_x = (i + shift_x) * scale;      // calculate the coordinates within the noise field
                uint32_t real_y = (i + shift_y) * scale;      // based on the precalculated positions
                uint32_t real_z = 4223;

                uint8_t noise = inoise16(real_x, real_y, real_z) >> 8; // get the noise data and scale it down

                uint8_t index = sin8(noise*3);                // map led color based on noise data
                uint8_t bri   = noise;

                leds[i] = ColorFromPalette(currentPalette, index, bri, LINEARBLEND); // With that value, look up the 8 bit colour palette value and assign it to the current LED.

        }

} // noise16_2()


// Use qsuba for smooth pixel colouring and qsubd for non-smooth pixel colouring
#define qsubd(x, b)  ((x>b) ? b : 0)                   // Digital unsigned subtraction macro. if result <0, then => 0. Otherwise, take on fixed value.
#define qsuba(x, b)  ((x>b) ? x-b : 0)                          // Analog Unsigned subtraction macro. if result <0, then => 0

// Initialize changeable global variables. Play around with these!!!
int8_t thisspeed = 8;                                         // You can change the speed of the wave, and use negative values.
uint8_t allfreq = 32;                                         // You can change the frequency, thus distance between bars.
int thisphase = 0;                                            // Phase change value gets calculated.
uint8_t thiscutoff = 192;                                     // You can change the cutoff value to display this wave. Lower value = longer wave.
uint8_t bgclr = 0;                                            // A rotating background colour.
uint8_t bgbright = 10;                                        // Brightness of background colour



void one_sine_pal(uint8_t colorIndex) {                                       // This is the heart of this program. Sure is short.
        thisdelay = 30;
        thisphase += thisspeed;                                               // You can change direction and speed individually.

        for (int k=0; k<numberLeds-1; k++) {                                    // For each of the LED's in the strand, set a brightness based on a wave as follows:
                int thisbright = qsubd(cubicwave8((k*allfreq)+thisphase), thiscutoff); // qsub sets a minimum value called thiscutoff. If < thiscutoff, then bright = 0. Otherwise, bright = 128 (as defined in qsub)..
                leds[k] = CHSV(bgclr, 255, bgbright);                         // First set a background colour, but fully saturated.
                leds[k] += ColorFromPalette( currentPalette, colorIndex, thisbright, currentBlending); // Let's now add the foreground colour.
                colorIndex +=3;
        }

        bgclr++;

} // one_sine_pal()

uint8_t thishue = 0;                                          // Starting hue value.
uint8_t deltahue = 10;

void rainbow_march() {                                        // The fill_rainbow call doesn't support brightness levels
        thishue++;                                            // Increment the starting hue.
        fill_rainbow(leds, numberLeds, thishue, deltahue);      // Use FastLED's fill_rainbow routine.
} // rainbow_march()


void fadeall() {
        for(int i = 0; i < numberLeds; i++) { leds[i].nscale8(250); }
}


void cylon() {
        static uint8_t hue = 0;
        Serial.print("x");
// First slide the led in one direction
        for(int i = 0; i < numberLeds; i++) {
                // Set the i'th led to red
                leds[i] = CHSV(hue++, 255, 255);
                // Show the leds
                FastLED.show();
                // now that we've shown the leds, reset the i'th led to black
                // leds[i] = CRGB::Black;
                fadeall();
                // Wait a little bit before we loop around and do it again
                delay(10);
        }
// Now go in the other direction.
        for(int i = (numberLeds)-1; i >= 0; i--) {
                // Set the i'th led to red
                leds[i] = CHSV(hue++, 255, 255);
                // Show the leds
                FastLED.show();
                // now that we've shown the leds, reset the i'th led to black
                // leds[i] = CRGB::Black;
                fadeall();
                // Wait a little bit before we loop around and do it again
                delay(10);
        }
}




void setup() {
        // put your setup code here, to run once:
        Serial.begin(115200);
        Serial.println("ESP-Neopixel V1.0");

        initEEPROM();

        // startup fastled
        stripSetup();
        setColor(red, green, blue);
        setBrightness(brightness);
        on();
        FastLED.show(); // display this frame


        wifi();

        // connect to mqtt
        client.setServer("192.168.1.2", 1883);
        client.setCallback(callback);
        reconnect();

        // check for software update
        checkUpdate();

        Serial.println("Setup completed.");


}

void checkConnectionState() {
        // reconnect if wifi drops
        if(WiFi.status() == 6) {
                wifi();
        }

        if(!client.connected()) {
                reconnect();
        }
}

void loop() {






        if(animation > 0) {
                // Handle MQTT subscription for animations slower

                EVERY_N_MILLISECONDS(500) {
                        checkConnectionState();
                        client.loop();

                }
                switch (animation) {
                case 1:
                        blur();
                        break;
                case 2:
                        dot_beat();
                        break;
                case 3:
                        EVERY_N_MILLISECONDS(100) {
                                uint8_t maxChanges = 24;
                                nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges); // AWESOME palette blending capability.
                        }

                        EVERY_N_SECONDS(5) {                // Change the target palette to a random one every 5 seconds.
                                targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 192, random8(128,255)), CHSV(random8(), 255, random8(128,255)));
                        }
                        beatwave();
                        break;
                case 4:
                        EVERY_N_MILLISECONDS(thisdelay) {                   // FastLED based non-blocking delay to update/display the sequence.
                                ease();
                        }
                        break;
                case 5:
                        blendme();
                        break;
                case 6:
                        ChangeMe();                                         // Check the demo loop for changes to the variables.
                        mover();                            // Call our sequence.
                        break;
                case 7:
                        EVERY_N_MILLISECONDS(50) {
                                nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges); // Blend towards the target palette
                        }

                        EVERY_N_SECONDS(5) { // Change the target palette to a random one every 5 seconds.
                                targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 192, random8(128,255)), CHSV(random8(), 255, random8(128,255)));
                        }

                        noise16_1();
                        break;
                case 8:
                        EVERY_N_MILLISECONDS(50) {
                                nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges); // Blend towards the target palette
                        }

                        EVERY_N_SECONDS(5) { // Change the target palette to a random one every 5 seconds.
                                targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 192, random8(128,255)), CHSV(random8(), 255, random8(128,255)));
                        }

                        noise16_2();
                        break;
                case 9:
                        //  EVERY_N_MILLISECONDS(thisdelay) {                           // FastLED based non-blocking delay to update/display the sequence.
                        EVERY_N_MILLISECONDS(thisdelay) {   // FastLED based non-blocking delay to update/display the sequence.
                                one_sine_pal(millis()>>4);
                        }

                        EVERY_N_MILLISECONDS(100) {
                                uint8_t maxChanges = 24;
                                nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges); // AWESOME palette blending capability.
                        }

                        EVERY_N_SECONDS(5) {                // Change the target palette to a random one every 5 seconds.
                                static uint8_t baseC = random8(); // You can use this as a baseline colour if you want similar hues in the next line.
                                targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 192, random8(128,255)), CHSV(random8(), 255, random8(128,255)));
                        }
                        break;
                case 10:
                        EVERY_N_MILLISECONDS(5) {                   // FastLED based non-blocking routine to update/display the sequence.
                                rainbow_march();
                        }
                        break;
                case 11:
                        cylon();
                        break;
                }


        }
        else {
                checkConnectionState();
                // Handle MQTT subscription
                client.loop();
        }


        // send the 'leds' array out to the actual LED strip
        FastLED.show();

}
