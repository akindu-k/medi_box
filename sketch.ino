#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire,OLED_RESET);

void setup(){
    Serial.begin(9600);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)){
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }

    display.display();
    delay(2000);
    display.clearDisplay();

    print_line("Welcome to Medobox", 10, 20, 2);
}

void loop(){

}

void print_line(String text, int text_size, int row, int column){

    display.clearDisplay();
    display.setTextSize(text_size);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(row,column);
    display.println(text);

    display.display();
    delay(2000);

}


