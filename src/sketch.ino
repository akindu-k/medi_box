#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <WiFi.h>
#include <DHTesp.h>

// Define OLED Display Parameters
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1
#define SCREEN_ADDRESS 0x3C
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// Define hardware pins
#define BUZZER 5
#define LED_1 15 
#define PB_CANCEL 34
#define PB_OK 32 
#define PB_UP 33
#define PB_DOWN 35
#define DHTPIN 12

// Time settings
#define NTP_SERVER     "pool.ntp.org"
#define UTC_OFFSET 19800  // 5 hours 30 minutes in seconds (5 * 3600 + 30 * 60)
#define UTC_OFFSET_DST 0  // No daylight saving time in Sri Lanka

// Declare objects
DHTesp dhtSensor;

// Variables
int hours, minutes, seconds;
int alarm_hours[] = {-1, -1};
int alarm_minutes[] = {-1, -1};
bool snooze_active = false;
unsigned long snooze_time = 0;
int LED2_blink;


// Function to display text at specified position and size
void print_line(String text, int x, int y, int size) {
  display.setTextSize(size);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(x, y);
  display.println(text);
}

// Function to update the display with current time and environment data
void print_time_now() {
  display.clearDisplay();
  
  // Format and display time
  char text[9];
  sprintf(text, "%02d:%02d:%02d", hours, minutes, seconds);
  print_line(text, 20, 0, 2);

  // Get sensor readings
  TempAndHumidity data = dhtSensor.getTempAndHumidity();

  // Display temperature and humidity
  print_line("Temperature: " + String(data.temperature) + " C", 0, 20, 1);
  print_line("Humidity:    " + String(data.humidity) + " %", 0, 30, 1);



  boolean temp_low = data.temperature < 24;
  boolean temp_high = data.temperature > 32;

  boolean humidity_low = data.humidity < 65;
  boolean humidity_high = data.humidity > 80;
  // Display appropriate warning messages
  if (temp_low){ 
      print_line("Temperature Low!", 0, 40, 1);
  }

  if (temp_high){
    print_line("Temperature High!", 0, 40, 1);
  }
  if (humidity_low){
    print_line("Humidity Low!", 0, 50, 1);
  }
  
  if (humidity_high){
    print_line("Humidity High!", 0, 50, 1);
  }
      

  display.display();

  boolean temp_warning = temp_low || temp_high;
  boolean humidity_warning = humidity_low || humidity_high; 

  // Handle warning indicators
  if (temp_warning || humidity_warning) {
      digitalWrite(LED_1, !digitalRead(LED_1));
      if (LED2_blink + 500 < millis()){
          digitalWrite(LED_1, !digitalRead(LED_1));
          LED2_blink = millis();

          // Alert sound for environmental warnings
          tone(BUZZER, 330);
          delay(300);
          noTone(BUZZER);
          delay(50);
      }
  } else { 
      digitalWrite(LED_1, LOW);
  }
}

// Function to play reminder melody
void play_melody() {
  // Musical note frequencies and durations
  int melody[] = { 262, 294, 330, 262, 330, 392, 440 };
  int noteDurations[] = { 300, 300, 300, 300, 400, 400, 500 };

  // Play each note in sequence
  for (int i = 0; i < 7; i++) {
      tone(BUZZER, melody[i]);
      delay(noteDurations[i]);
      noTone(BUZZER);
      delay(2);
  }
}

void ring_alarm() {
    display.clearDisplay();
    print_line("MEDICINE", 25, 5, 2);
    print_line("TIME!", 40, 20, 2);
    print_line("OK: Remind in 5 min", 10, 40, 1);
    print_line("Cancel: Dismiss", 10, 50, 1);
    display.display();
  
    // Visual indicator
    digitalWrite(LED_1, HIGH);
  
    // Wait for user response
    while (true) {
        play_melody();
        digitalWrite(LED_1, !digitalRead(LED_1));
  
        // Handle snooze request
        if (digitalRead(PB_OK) == LOW) {
            snooze_active = true;
            snooze_time = millis() + 300000; // 5 minutes delay
  
            display.clearDisplay();
            print_line("Alarm snoozed for", 10, 20, 1);
            print_line("5 minutes.", 10, 30, 1);
            display.display();
            delay(1000);
            break;
        }
  
        // Handle dismissal
        if (digitalRead(PB_CANCEL) == LOW) {
            display.clearDisplay();
            print_line("Alarm", 10, 20, 2);
            print_line("Snoozed", 10, 40, 2);
            display.display();
            delay(1000);
            break;
        }
    }
  
    // Reset alert state
    noTone(BUZZER);
    digitalWrite(LED_1, LOW);
    display.clearDisplay();
    display.display();
  }

// Function to monitor time and check for scheduled reminders
void update_time_with_check_alarm() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;
    
    // Update current time
    hours = timeinfo.tm_hour;
    minutes = timeinfo.tm_min;
    seconds = timeinfo.tm_sec;
    
    print_time_now();

    // Check for scheduled reminders
    for (int i = 0; i < 2; i++) {
        if (hours == alarm_hours[i] && minutes == alarm_minutes[i] && seconds == 0) {
            ring_alarm();
        }
    }

    // Check for snoozed reminders
    if (snooze_active && millis() >= snooze_time) {
        snooze_active = false;
        ring_alarm();
    }
}

// Function to configure time zone settings
void set_time() {
  int new_hours = UTC_OFFSET / 3600;
  int new_minutes = UTC_OFFSET % 3600 / 60;
  bool setting_minutes = false;

  while (true) {
      display.clearDisplay();
      print_line("Time Zone Config:", 0, 0, 2);
      print_line(String(new_hours) + ":" + (new_minutes < 10 ? "0" : "") + String(new_minutes), 20, 20, 2);
      print_line(setting_minutes ? "Setting Minutes" : "Setting Hours", 0, 40, 1);
      print_line("Press Cancel to Exit", 0, 50, 1);
      display.display();

      // Handle UP button
      if (digitalRead(PB_UP) == LOW) {
        if (!setting_minutes) {
            new_hours = (new_hours == 14) ? -12 : new_hours + 1;
        } else {
            new_minutes = (new_minutes + 1) % 60;
        }
        delay(200);
      }

      // Handle DOWN button
      if (digitalRead(PB_DOWN) == LOW) {
        if (!setting_minutes) {
            new_hours = (new_hours == -12) ? 14 : new_hours - 1;
        } else {
            new_minutes = (new_minutes == 0) ? 59 : new_minutes - 1;
        }
        delay(200);
      }

      // Handle OK button
      if (digitalRead(PB_OK) == LOW) {
          delay(200);
          if (!setting_minutes) {
              setting_minutes = true;
          } else {
            int new_utc_offset = new_hours * 3600 + new_minutes * 60;
            configTime(new_utc_offset, UTC_OFFSET_DST, NTP_SERVER);
            
            display.clearDisplay();
            print_line("Time Zone", 0, 0, 2);
            print_line("Updated", 0, 20, 2);
            display.display();
            delay(1000);
            break;
          }
      }
      
      // Handle CANCEL button
      if (digitalRead(PB_CANCEL) == LOW) {
          delay(200);
          break;
      }
  }
}

// Function to set medication reminders
void set_alarm() {
  int alarm_index = 0;
  int new_hours = hours;
  int new_minutes = minutes;
  bool setting_minutes = false;
  bool selecting_alarm = true;

  while (true) {
      display.clearDisplay();
      
      if (selecting_alarm) {
          print_line("Select Alarm Slot", 5, 0, 1);
          print_line((alarm_index == 0 ? "-> " : "  ") + String("Alarm 1"), 0, 20, 1);
          print_line((alarm_index == 1 ? "-> " : "  ") + String("Alarm 2"), 0, 30, 1);
      } else {
          print_line("Alarm " + String(alarm_index + 1), 10, 0, 1);
          print_line(String(new_hours) + ":" + (new_minutes < 10 ? "0" : "") + String(new_minutes), 30, 20, 1);
          print_line(setting_minutes ? "Setting Minutes" : "Setting Hours", 0, 40, 1);
      }
      display.display();

      // Handle UP button
      if (digitalRead(PB_UP) == LOW) {
          if (selecting_alarm) {
              alarm_index = (alarm_index == 0) ? 1 : 0;
          } else {
              if (!setting_minutes) new_hours = (new_hours + 1) % 24;
              else new_minutes = (new_minutes + 1) % 60;
          }
          delay(200);
      }

      // Handle DOWN button
      if (digitalRead(PB_DOWN) == LOW) {  // Check if the "down" button is pressed
        delay(50);  // Debounce delay
        if (digitalRead(PB_DOWN) == LOW) {  // Ensure button is still pressed after debounce
            if (selecting_alarm) {  
                // Toggle between two alarm indexes (assuming only 0 and 1 exist)
                alarm_index = (alarm_index == 0) ? 1 : 0;
            } else {  
                // If not selecting an alarm, adjust time settings
                if (!setting_minutes) {  
                    // Adjust hours (wraps around from 0 to 23)
                    new_hours = (new_hours == 0) ? 23 : new_hours - 1;
                } else {  
                    // Adjust minutes (wraps around from 0 to 59)
                    new_minutes = (new_minutes == 0) ? 59 : new_minutes - 1;
                }
            }
            delay(200);  // Delay to prevent rapid changes
            while (digitalRead(PB_DOWN) == LOW) { delay(10); }  // Wait for button release
        }
    }
    

      // Handle OK button
      if (digitalRead(PB_OK) == LOW) {
          delay(200);
          if (selecting_alarm) {
              selecting_alarm = false; 
          } else if (!setting_minutes) {
              setting_minutes = true; 
          } else {
              alarm_hours[alarm_index] = new_hours;
              alarm_minutes[alarm_index] = new_minutes;
              break;
          }
      }

      // Handle CANCEL button
      if (digitalRead(PB_CANCEL) == LOW) {
          delay(200);
          break;
      }
  }
}

// Function to view configured reminders
void view_alarms() {
  while (true) {
      display.clearDisplay();
      print_line("Current Alarms", 0, 0, 1);

      for (int i = 0; i < 2; i++) {
          if (alarm_hours[i] != -1) {
              String alarmText = "Alarm " + String(i + 1) + ": " + 
                                 String(alarm_hours[i]) + ":" + 
                                 (alarm_minutes[i] < 10 ? "0" : "") + String(alarm_minutes[i]);
              print_line(alarmText, 0, 15 + (i * 10), 1);
          } else {
              print_line("Alarm " + String(i + 1) + ": Not Set", 0, 15 + (i * 10), 1);
          }
      }

      print_line("Press Cancel to exit", 0, 45, 1);
      display.display();

      // Exit on CANCEL button press
      if (digitalRead(PB_CANCEL) == LOW) {
          delay(200);
          break;
      }
  }
}

// Function to remove configured reminders
void delete_alarm() {
  int alarm_index = 0;

  while (true) {
      display.clearDisplay();
      print_line("Deleted Alarm", 0, 0, 1);
      
      for (int i = 0; i < 2; i++) {
          if (i == alarm_index) {
              print_line("-> Alarm " + String(i + 1) + ": " + 
                         String(alarm_hours[i]) + ":" + 
                         (alarm_minutes[i] < 10 ? "0" : "") + String(alarm_minutes[i]), 0, 15 + (i * 10), 1);
          } else {
              print_line("Alarm " + String(i + 1) + ": " + 
                         String(alarm_hours[i]) + ":" + 
                         (alarm_minutes[i] < 10 ? "0" : "") + String(alarm_minutes[i]), 0, 15 + (i * 10), 1);
          }
      }

      print_line("OK to Remove", 0, 40, 1);
      print_line("Cancel to Exit", 0, 50, 1);
      display.display();

      // Handle UP/DOWN navigation
      if (digitalRead(PB_UP) == LOW || digitalRead(PB_DOWN) == LOW) {
          alarm_index = 1 - alarm_index; // Toggle between 0 and 1
          delay(200);
      }

      // Handle deletion confirmation
      if (digitalRead(PB_OK) == LOW) {
          alarm_hours[alarm_index] = -1;
          alarm_minutes[alarm_index] = -1;
          
          display.clearDisplay();
          print_line("Alarm", 0, 20, 2);
          print_line("Removed", 0, 40, 2);
          display.display();
          delay(1000);
          break;
      }

      // Handle cancellation
      if (digitalRead(PB_CANCEL) == LOW) {
          delay(200);
          break;
      }
  }
}

// Function for main menu navigation
void go_to_menu() {
  int menu_option = 1;

  while (true) {
      display.clearDisplay();

      // Display menu options with selection indicator
      if (menu_option == 1) {
          print_line("* Set Time Zone", 0, 0, 1);
      } else {
          print_line("1. Set Time Zone", 0, 0, 1);
      }

      if (menu_option == 2) {
          print_line("* Set Alarms", 0, 10, 1);
      } else {
          print_line("2. Set Alarms", 0, 10, 1);
      }

      if (menu_option == 3) {
          print_line("* View Alarms", 0, 20, 1);
      } else {
          print_line("3. View Alarms", 0, 20, 1);
      }

      if (menu_option == 4) {
          print_line("* Delete Alarms", 0, 30, 1);
      } else {
          print_line("4. Delete Alarms", 0, 30, 1);
      }

      if (menu_option == 5) {
          print_line("* Exit", 0, 40, 1);
      } else {
          print_line("5. Exit", 0, 40, 1);
      }

      display.display();

      // Handle UP navigation
      if (digitalRead(PB_UP) == LOW) {
          menu_option = (menu_option == 1) ? 5 : menu_option - 1;
          delay(200);
      }
      
      // Handle DOWN navigation
      if (digitalRead(PB_DOWN) == LOW) {
          menu_option = (menu_option == 5) ? 1 : menu_option + 1;
          delay(200);
      }

      // Handle menu selection
      if (digitalRead(PB_OK) == LOW) {
          delay(200);
          switch (menu_option) {
              case 1: set_time(); break;
              case 2: set_alarm(); break;
              case 3: view_alarms(); break;
              case 4: delete_alarm(); break;
              case 5: return; // Exit menu
          }
      }

      // Handle menu exit
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

    // Initialize display
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
        print_line("Connecting to WiFi...", 0, 0, 1);
        display.display();
        delay(100);
    }
    
    // Show network connection info
    display.clearDisplay();
    print_line("WiFi Connected", 0, 0, 1);
    display.display();
    delay(2000);
    
    // Initialize time
    configTime(UTC_OFFSET, UTC_OFFSET_DST, NTP_SERVER);
}

void loop() {
    update_time_with_check_alarm();
    
    if (digitalRead(PB_OK) == LOW) {
        delay(200);
        go_to_menu();
    }
}