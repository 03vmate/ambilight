#include <Arduino.h>
#include <Adafruit_NeoPixel.h>

#define LED_COUNT 128
#define RECV_BUF_SIZE (LED_COUNT * 2)

Adafruit_NeoPixel pixels(LED_COUNT, 21, NEO_GRB + NEO_KHZ800);

int serPos = 0;
char receiveBuffer[RECV_BUF_SIZE];

void setup() {
  pixels.begin();
  pixels.show();
  pixels.setBrightness(255);

  //Baud rate is critical - 115200 is just barely fast enough for about 100 LEDs. (115200/8/(100*3+1)) = 47.8 FPS max. 921600 should be able to do 921600/8/(100*3+1) = 382.7 FPS max.
  Serial.begin(921600);
}

void lineReceived(uint len) {
  //Only accept LED_COUNT*3 (R,G,B) bytes, otherwise ignore as input malformed
  if(len == LED_COUNT * 3) {
    for(int i = 0; i < 128; i++) {
      pixels.setPixelColor(i, pixels.Color(receiveBuffer[i*3], receiveBuffer[i*3+1], receiveBuffer[i*3+2]));
    }
    pixels.show();
  }
}

void loop() {
  while(Serial.available()) {
    //Essentially just read into a rolling buffer continously, call lineReceived when we get a newline, and then reset to pos=0
    char recv = Serial.read();
    if(recv == '\n') {
      lineReceived(serPos);
      serPos = 0;
    }
    else {
      //roll over at the end of the buffer
      if(serPos >= RECV_BUF_SIZE - 1) {
        serPos = 0;
      }
      receiveBuffer[serPos] = recv;
      serPos++;
    }
  }
}
