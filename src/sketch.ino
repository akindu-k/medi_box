#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

#define BUZZER 5
#define LED_1 15
#define PB_CANCEL 34

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Global variables
int days = 0;
int hours = 0;
int minutes = 0;
int seconds = 0;

unsigned long timeNow = 0;
unsigned long timeLast = 0;

bool alarm_enabled = true;
int n_alarms = 2;
int alarm_hours[] = {0, 1};
int alarm_minutes[] = {1, 10};
bool alarm_triggered[] = {false, false};

int n_notes = 8;
int C = 262;
int D = 294;
int E = 330;
int F = 349;
int G = 392;
int A = 440;
int B = 494;
int C_H = 523;
int notes[] = {C, D, E, F, G, A, B, C_H};

void setup() {
    pinMode(BUZZER, OUTPUT);
    pinMode(LED_1, OUTPUT);
    pinMode(PB_CANCEL,INPUT);
    Serial.begin(9600);

    // Initialize the OLED display
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println(F("SSD1306 allocation failed"));
        for (;;); // Halt execution if display initialization fails
    }

    display.display();
    delay(2000); // Display splash screen for 2 seconds
    display.clearDisplay();

    // Display welcome message
    print_line("Welcome to Medobox!", 0, 0, 2);
    display.clearDisplay();
}

void loop() {
    update_time_with_check_alarm();
}

void print_line(String text, int column, int row, int text_size) {
    display.clearDisplay(); // Clear the display before printing
    display.setTextSize(text_size);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(column, row);
    display.println(text);
    display.display();
}

void print_time_now(void) {
    // Print the current time in the format: days:hours:minutes:seconds
    print_line(String(days), 0, 0, 2);
    print_line(":", 20, 0, 2);
    print_line(String(hours), 30, 0, 2);
    print_line(":", 50, 0, 2);
    print_line(String(minutes), 60, 0, 2);
    print_line(":", 80, 0, 2);
    print_line(String(seconds), 90, 0, 2);
}

void update_time(void) {
    timeNow = millis() / 1000; // Get the number of seconds since the program started
    seconds = timeNow - timeLast; // Calculate elapsed seconds since last update

    if (seconds >= 60) {
        minutes++; // Increment minutes if 60 seconds have passed
        timeLast += 60; // Adjust timeLast to account for the elapsed minute
    }

    if (minutes == 60) {
        hours++; // Increment hours if 60 minutes have passed
        minutes = 0; // Reset minutes
    }

    if (hours == 24) {
        days++; // Increment days if 24 hours have passed
        hours = 0; // Reset hours
    }
}

void ring_alarm() {
    display.clearDisplay();
    print_line("MEDICINE TIME!", 0, 0, 2);

    digitalWrite(LED_1, HIGH);

    bool break_happened = false;

    // Ring the buzzer
    while (break_happened == false && digitalRead(PB_CANCEL) == HIGH) {
        for (int i = 0; i < n_notes; i++) {
            if (digitalRead(PB_CANCEL) == LOW) {
                delay(200);
                break_happened = true;
                break;
            }
            tone(BUZZER, notes[i]);
            delay(500);
            noTone(BUZZER);
            delay(2);
        }
    }

    digitalWrite(LED_1, LOW);
    display.clearDisplay();
}

// handles all the cases
void update_time_with_check_alarm(void) {
    update_time(); // Update the time variables
    print_time_now(); // Display the current time
    
    if (alarm_enabled == true) {
        for (int i = 0; i < n_alarms; i++) {
            if (alarm_triggered[i] == false && alarm_hours[i] == hours && alarm_minutes[i] == minutes) {
                ring_alarm();
                alarm_triggered[i] = true;
            }
        }
    }
}
