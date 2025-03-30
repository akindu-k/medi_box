#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <DHTesp.h>

// Constants
#define NTP_SERVER "pool.ntp.org"
#define UTC_OFFSET 19800
#define UTC_OFFSET_DST 0

// Display settings
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C

// Pin definitions
#define BUZZER 5
#define LED_1 15 
#define PB_CANCEL 34
#define PB_OK 32 
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12

// Temperature thresholds
#define TEMP_LOW_THRESHOLD 24
#define TEMP_HIGH_THRESHOLD 32
#define HUMIDITY_LOW_THRESHOLD 65
#define HUMIDITY_HIGH_THRESHOLD 80

// Objects
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
DHTesp dhtSensor;

// Global variables
struct {
  int hours;
  int minutes;
  int seconds;
} currentTime;

struct Alarm {
  int hours;
  int minutes;
  bool isSet;
} alarms[2];

struct {
  bool isActive;
  unsigned long endTime;
} snooze;

unsigned long lastLedBlinkTime = 0;
const int SNOOZE_DURATION_MS = 300000; // 5 minutes

// Function prototypes
void printLine(const String& text, int x, int y, int size);
void displayCurrentTime();
void playMelody();
void ringAlarm();
bool updateTimeAndCheckAlarms();
void setTimeZone();
void setAlarm();
void viewAlarms();
void deleteAlarm();
void showMenu();
void handleWarnings(bool tempWarning, bool humidityWarning);
String formatTime(int hours, int minutes);

// Helper function to print text on the OLED display
void printLine(const String& text, int x, int y, int size) {
    display.setTextSize(size);
    display.setTextColor(SSD1306_WHITE);
    display.setCursor(x, y);
    display.println(text);
}

// Displays the current time and environmental data
void displayCurrentTime() {
    display.clearDisplay();
    
    // Format and display time
    char timeText[9];
    sprintf(timeText, "%02d:%02d:%02d", currentTime.hours, currentTime.minutes, currentTime.seconds);
    printLine(timeText, 20, 0, 2);
    
    // Get and display temperature and humidity
    TempAndHumidity data = dhtSensor.getTempAndHumidity();
    printLine("Temperature: " + String(data.temperature) + " C", 0, 20, 1);
    printLine("Humidity:    " + String(data.humidity) + " %", 0, 30, 1);

    // Check for environmental warnings
    boolean tempLow = data.temperature < TEMP_LOW_THRESHOLD;
    boolean tempHigh = data.temperature > TEMP_HIGH_THRESHOLD;
    boolean humidityLow = data.humidity < HUMIDITY_LOW_THRESHOLD;
    boolean humidityHigh = data.humidity > HUMIDITY_HIGH_THRESHOLD;

    // Display appropriate warnings
    if (tempLow) printLine("TEMP LOW!", 0, 40, 1);
    if (tempHigh) printLine("TEMP HIGH!", 0, 40, 1);
    if (humidityLow) printLine("HUMIDITY LOW!", 0, 50, 1);
    if (humidityHigh) printLine("HUMIDITY HIGH!", 0, 50, 1);
    
    display.display();

    // Handle warning indicators
    boolean tempWarning = tempLow || tempHigh;
    boolean humidityWarning = humidityLow || humidityHigh;
    handleWarnings(tempWarning, humidityWarning);
}

void handleWarnings(bool tempWarning, bool humidityWarning) {
    if (tempWarning || humidityWarning) {
        if (millis() - lastLedBlinkTime >= 500) {
            digitalWrite(LED_1, !digitalRead(LED_1));
            lastLedBlinkTime = millis();
            tone(BUZZER, 330);
            delay(300);
            noTone(BUZZER);
            delay(50);
        }
    } else {
        digitalWrite(LED_1, LOW);
    }
}

// Plays a predefined melody on the buzzer
void playMelody() {
    const int melodySize = 7;
    int melody[melodySize] = { 262, 294, 330, 262, 330, 392, 440 };
    int noteDurations[melodySize] = { 300, 300, 300, 300, 400, 400, 500 };
    
    for (int i = 0; i < melodySize; i++) {
        tone(BUZZER, melody[i]);
        delay(noteDurations[i]);
        noTone(BUZZER);
        delay(2);
    }
}

// Handles the alarm ringing logic
void ringAlarm() {
    display.clearDisplay();
    printLine("MEDICINE", 25, 5, 2);
    printLine("TIME!", 40, 20, 2);
    printLine("OK: Snooze for 5 min", 10, 40, 1);
    printLine("Cancel: Dismiss", 10, 50, 1);
    display.display();
    digitalWrite(LED_1, HIGH);

    // Wait for user input to snooze or dismiss the alarm
    while (true) {
        playMelody();
        digitalWrite(LED_1, !digitalRead(LED_1));
        
        if (digitalRead(PB_OK) == LOW) {
            snooze.isActive = true;
            snooze.endTime = millis() + SNOOZE_DURATION_MS;
            
            display.clearDisplay();
            printLine("Alarm snoozed for", 10, 20, 1);
            printLine("5 minutes.", 10, 30, 1);
            display.display();
            delay(1000);
            break;
        }
        
        if (digitalRead(PB_CANCEL) == LOW) {
            display.clearDisplay();
            printLine("Alarm", 10, 20, 2);
            printLine("Dismissed", 10, 40, 2);
            display.display();
            delay(1000);
            break;
        }
    }
    
    noTone(BUZZER);
    digitalWrite(LED_1, LOW);
    display.clearDisplay();
    display.display();
}

// Updates the current time and checks if any alarms need to ring
bool updateTimeAndCheckAlarms() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return false;
    }
    
    currentTime.hours = timeinfo.tm_hour;
    currentTime.minutes = timeinfo.tm_min;
    currentTime.seconds = timeinfo.tm_sec;
    
    displayCurrentTime();

    // Check if any alarm should ring
    for (int i = 0; i < 2; i++) {
        if (alarms[i].isSet && 
            currentTime.hours == alarms[i].hours && 
            currentTime.minutes == alarms[i].minutes && 
            currentTime.seconds == 0) {
            ringAlarm();
        }
    }

    // Check if snooze time has elapsed
    if (snooze.isActive && millis() >= snooze.endTime) {
        snooze.isActive = false;
        ringAlarm();
    }
    
    return true;
}

// Format time as HH:MM with leading zeros
String formatTime(int hours, int minutes) {
    char timeStr[6];
    sprintf(timeStr, "%02d:%02d", hours, minutes);
    return String(timeStr);
}

// Allows the user to set the time zone offset
void setTimeZone() {
    int newHours = UTC_OFFSET / 3600;
    int newMinutes = (UTC_OFFSET % 3600) / 60;
    bool settingMinutes = false;

    while (true) {
        display.clearDisplay();
        printLine("Set Time Zone", 0, 0, 1);
        
        char timeText[6];
        sprintf(timeText, "%+03d:%02d", newHours, newMinutes);
        printLine(timeText, 20, 20, 2);
        
        printLine(settingMinutes ? "Adjust Minutes" : "Adjust Hours", 0, 40, 1);
        printLine("Cancel to Exit", 0, 50, 1);
        display.display();

        // Handle button inputs
        if (digitalRead(PB_UP) == LOW) {
            if (settingMinutes) {
                newMinutes = (newMinutes + 1) % 60;
            } else {
                newHours = (newHours == 14) ? -12 : newHours + 1;
            }
            delay(200);
        }
        
        if (digitalRead(PB_DOWN) == LOW) {
            if (settingMinutes) {
                newMinutes = (newMinutes == 0) ? 59 : newMinutes - 1;
            } else {
                newHours = (newHours == -12) ? 14 : newHours - 1;
            }
            delay(200);
        }
        
        if (digitalRead(PB_OK) == LOW) {
            delay(200);
            if (settingMinutes) {
                int newUtcOffset = newHours * 3600 + newMinutes * 60;
                configTime(newUtcOffset, UTC_OFFSET_DST, NTP_SERVER);
                
                display.clearDisplay();
                printLine("Time Zone Set", 10, 20, 2);
                display.display();
                delay(1000);
                break;
            } else {
                settingMinutes = true;
            }
        }
        
        if (digitalRead(PB_CANCEL) == LOW) {
            delay(200);
            break;
        }
    }
}

// Allows the user to set alarms
void setAlarm() {
    int alarmIndex = 0;
    int newHours = currentTime.hours;
    int newMinutes = currentTime.minutes;
    bool settingMinutes = false;
    bool selectingAlarm = true;

    // Loop to configure alarms
    while (true) {
        display.clearDisplay();
        
        if (selectingAlarm) {
            printLine("Select Alarm", 5, 0, 1);
            printLine((alarmIndex == 0 ? "* " : "  ") + String("Alarm 1"), 0, 20, 1);
            printLine((alarmIndex == 1 ? "* " : "  ") + String("Alarm 2"), 0, 30, 1);
        } else {
            printLine("Alarm " + String(alarmIndex + 1), 10, 0, 1);
            printLine(formatTime(newHours, newMinutes), 30, 20, 1);
            printLine(settingMinutes ? "Setting Minutes" : "Setting Hours", 0, 40, 1);
        }
        
        display.display();

        // Handle user input for alarm configuration
        if (digitalRead(PB_UP) == LOW) {
            if (selectingAlarm) {
                alarmIndex = (alarmIndex == 0) ? 1 : 0;
            } else if (!settingMinutes) {
                newHours = (newHours + 1) % 24;
            } else {
                newMinutes = (newMinutes + 1) % 60;
            }
            delay(200);
        }
        
        if (digitalRead(PB_DOWN) == LOW) {
            if (selectingAlarm) {
                alarmIndex = (alarmIndex == 0) ? 1 : 0;
            } else if (!settingMinutes) {
                newHours = (newHours == 0) ? 23 : newHours - 1;
            } else {
                newMinutes = (newMinutes == 0) ? 59 : newMinutes - 1;
            }
            delay(200);
        }
        
        if (digitalRead(PB_OK) == LOW) {
            delay(200);
            if (selectingAlarm) {
                selectingAlarm = false;
            } else if (!settingMinutes) {
                settingMinutes = true;
            } else {
                alarms[alarmIndex].hours = newHours;
                alarms[alarmIndex].minutes = newMinutes;
                alarms[alarmIndex].isSet = true;
                break;
            }
        }
        
        if (digitalRead(PB_CANCEL) == LOW) {
            delay(200);
            break;
        }
    }
}

// Displays the list of currently set alarms
void viewAlarms() {
    while (true) {
        display.clearDisplay();
        printLine("Current Alarms", 0, 0, 1);
        
        for (int i = 0; i < 2; i++) {
            if (alarms[i].isSet) {
                String alarmText = "Alarm " + String(i + 1) + ": " + formatTime(alarms[i].hours, alarms[i].minutes);
                printLine(alarmText, 0, 15 + (i * 10), 1);
            } else {
                printLine("Alarm " + String(i + 1) + ": Not Set", 0, 15 + (i * 10), 1);
            }
        }
        
        printLine("Press Cancel to exit", 0, 45, 1);
        display.display();
        
        if (digitalRead(PB_CANCEL) == LOW) {
            delay(200);
            break;
        }
    }
}

// Allows the user to delete alarms
void deleteAlarm() {
    int alarmIndex = 0;
    
    while (true) {
        display.clearDisplay();
        printLine("Delete Alarm", 0, 0, 1);
        
        for (int i = 0; i < 2; i++) {
            String prefix = (i == alarmIndex) ? "* " : "  ";
            String status;
            
            if (alarms[i].isSet) {
                status = formatTime(alarms[i].hours, alarms[i].minutes);
            } else {
                status = "Not Set";
            }
            
            printLine(prefix + "Alarm " + String(i + 1) + ": " + status, 0, 15 + (i * 10), 1);
        }
        
        printLine("OK to Remove", 0, 40, 1);
        printLine("Cancel to Exit", 0, 50, 1);
        display.display();

        // Handle user input for alarm deletion
        if (digitalRead(PB_UP) == LOW || digitalRead(PB_DOWN) == LOW) {
            alarmIndex = 1 - alarmIndex;
            delay(200);
        }
        
        if (digitalRead(PB_OK) == LOW) {
            alarms[alarmIndex].isSet = false;
            
            display.clearDisplay();
            printLine("Alarm", 0, 20, 2);
            printLine("Removed", 0, 40, 2);
            display.display();
            delay(1000);
            break;
        }
        
        if (digitalRead(PB_CANCEL) == LOW) {
            delay(200);
            break;
        }
    }
}

// Main menu for user interaction
void showMenu() {
    const int MENU_ITEMS = 5;
    int menuOption = 1;
    
    while (true) {
        display.clearDisplay();
        printLine("Menu", 0, 0, 1);
        
        // Display menu options
        const char* menuLabels[MENU_ITEMS] = {
            "Set Time Zone", "Set Alarms", "View Alarms", "Delete Alarms", "Exit"
        };
        
        for (int i = 0; i < MENU_ITEMS; i++) {
            String prefix = (i + 1 == menuOption) ? "* " : "";
            printLine(prefix + String(i + 1) + ". " + menuLabels[i], 0, 10 + (i * 10), 1);
        }
        
        display.display();

        // Handle user input for menu navigation
        if (digitalRead(PB_UP) == LOW) {
            menuOption = (menuOption == 1) ? MENU_ITEMS : menuOption - 1;
            delay(200);
        }
        
        if (digitalRead(PB_DOWN) == LOW) {
            menuOption = (menuOption == MENU_ITEMS) ? 1 : menuOption + 1;
            delay(200);
        }
        
        if (digitalRead(PB_OK) == LOW) {
            delay(200);
            
            if (menuOption == 5) {
                return;
            }
            
            // Execute selected menu option
            switch (menuOption) {
                case 1: setTimeZone(); break;
                case 2: setAlarm(); break;
                case 3: viewAlarms(); break;
                case 4: deleteAlarm(); break;
            }
        }
        
        if (digitalRead(PB_CANCEL) == LOW) {
            delay(200);
            return;
        }
    }
}

void setup() {
    Serial.begin(9600);
    
    // Initialize pins
    pinMode(BUZZER, OUTPUT);
    pinMode(LED_1, OUTPUT); 
    pinMode(PB_CANCEL, INPUT_PULLUP);
    pinMode(PB_OK, INPUT_PULLUP);
    pinMode(PB_UP, INPUT_PULLUP);
    pinMode(PB_DOWN, INPUT_PULLUP);
    
    // Initialize DHT sensor
    dhtSensor.setup(DHTPIN, DHTesp::DHT22);

    // Initialize alarms
    for (int i = 0; i < 2; i++) {
        alarms[i].isSet = false;
    }
    
    // Initialize snooze
    snooze.isActive = false;

    // Initialize the OLED display
    if (!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
        Serial.println("SSD1306 allocation failed");
        while (true);
    }
    display.display();
    delay(2000);

    // Connect to WiFi
    WiFi.begin("Wokwi-GUEST", "");
    while (WiFi.status() != WL_CONNECTED) {
        display.clearDisplay();
        printLine("Connecting to WiFi...", 0, 0, 1);
        display.display();
        delay(100);
    }
    
    display.clearDisplay();
    printLine("WiFi Connected", 0, 0, 1);
    display.display();
    delay(2000);

    // Configure time synchronization
    configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
}

void loop() {
    updateTimeAndCheckAlarms();

    // Enter menu if OK button is pressed
    if (digitalRead(PB_OK) == LOW) {
        delay(200);
        showMenu();
    }
}