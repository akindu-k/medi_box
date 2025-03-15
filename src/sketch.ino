#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

Adafruit_SSD1306 display(SCREEN_WIDTH,SCREEN_HEIGHT,&Wire,OLED_RESET);

// global variables

int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

unsigned long timeNow = 0;
unsigned long timeLast = 0;

void setup(){
    Serial.begin(9600);

    // Initialize the OLED display
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)){
        Serial.println(F("SSD1306 allocation failed"));
        for(;;); // Halt execution if display initialization fails
    }

    display.display();
    delay(2000); // Display splash screen for 2 seconds
    display.clearDisplay();

    // Display welcome message
    print_line("Welcome to Medobox!", 0, 0, 2);
    display.clearDisplay();
}

void loop(){
    update_time(); // Update the time variables
    print_time_now(); // Display the current time
}

void print_line(String text, int column, int row, int text_size){
    display.clearDisplay(); // Clear the display before printing
    display.setTextSize(text_size);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(column,row);
    display.println(text);
    display.display();
}

void print_time_now(void){
    // Print the current time in the format: days:hours:minutes:seconds
    print_line(String(days), 0, 0, 2);
    print_line(":", 20, 0, 2);
    print_line(String(hours), 30, 0, 2);
    print_line(":", 50, 0, 2);
    print_line(String(minutes), 60, 0, 2);
    print_line(":", 80, 0, 2);
    print_line(String(seconds), 90, 0, 2);
}

void update_time(void){
    timeNow = millis()/1000; // Get the number of seconds since the program started
    seconds = timeNow - timeLast; // Calculate elapsed seconds since last update

    if (seconds >= 60){
        minutes++; // Increment minutes if 60 seconds have passed
        timeLast += 60; // Adjust timeLast to account for the elapsed minute
    }

    if (minutes == 60){
        hours++; // Increment hours if 60 minutes have passed
        minutes = 0; // Reset minutes
    }

    if (hours == 24){
        days++; // Increment days if 24 hours have passed
        hours = 0; // Reset hours
    }
}

