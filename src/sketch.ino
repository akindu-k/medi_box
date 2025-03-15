#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire,OLED_RESET);

int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

unsigned long timeNow = 0;
unsigned long timeLast = 0;

void setup(){
    Serial.begin(9600);

    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)){
        Serial.println(F("SSD1306 allocation failed"));
        for(;;);
    }

    display.display();
    delay(2000);
    display.clearDisplay();

    print_line("Welcome to Medobox!", 0, 0, 2);
    display.clearDisplay();
}

void loop(){
    update_time();
    print_time_now();
}

void print_line(String text, int column, int row, int text_size){
    display.clearDisplay();
    display.setTextSize(text_size);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(column,row);
    display.println(text);
    display.display();
}

void print_time_now(void){
    print_line(String(days), 0, 0, 2);
    print_line(":", 20, 0, 2);
    print_line(String(hours), 30, 0, 2);
    print_line(":", 50, 0, 2);
    print_line(String(minutes), 60, 0, 2);
    print_line(":", 80, 0, 2);
    print_line(String(seconds), 90, 0, 2);
}

void update_time(void){
    timeNow = millis()/1000; // seconds passed from boot up
    seconds = timeNow - timeLast;

    if (seconds >= 60){
        minutes++;
        timeLast += 60;
    }

    if (minutes == 60){
        hours++;
        minutes = 0;
    }

    if (hours == 24){
        days++;
        hours = 0;
    }

}


