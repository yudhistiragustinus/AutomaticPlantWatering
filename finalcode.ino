/*
 *  Author: George Bantique (Menu Control Backbone only)
 *          @TechToTinker 
 *          July 18, 2020 
 *  Author:  Agustinus Yudhistira W. S. (For EL2142 Final Project)
 *           18020005 | October 31, 2021   
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include "Sodaq_DS3231.h"

LiquidCrystal_I2C lcd(0x27, 20, 4);

// ================================== VARIABEL =============================================

/*
 * B = Back
 * D = Down
 * U = Up
 * E = Enter/Select
 * hpl = LED/lamp
 * soil = soil moisture sensor
 * pump = pompa air
 */

#define button_B 2
#define button_D 3
#define button_U 4
#define button_E 5
#define hpl 6
#define soil A0
#define pump 9

#define DEFAULT_DELAY 300

//pembacaan hari
char weekDay[][4] = {"Sun", "Mon", "Tue", "Wed", "Thu", "Fri", "Sat"};

char buttonPressed = '0';

byte menuLevel = 0; // Level 0: no menu display, tampilan awal
                    // Level 1: display main menu, sebelum ke menu setting
                    // Level 2: display sub menu, di menu setting

byte menu = 1;
// byte sub = 1;

//mode led manual
int ledMode = 0;
int soilValue;
bool manLed = false;
int appliedMode;
//kalibrasi untuk moisture sensor
const int dry = 595;
const int wet = 239;
//variabel untuk waktu berdasarkan rtc
int year, month, date, hour, minute, second;
char *day;
//variabel untuk setting waktu
int h2, m2, s2;
//variabel untuk setting waktu manual
int timeSet = 0;
bool timeSetting = false;
//variabel untuk sensor suhu dari RTC
int temp;
//variabel untuk mapping kelembapan
int moist;
bool activeHigh = false;
bool automaticPump = true;

bool currState_B = HIGH;
bool currState_D = HIGH;
bool currState_U = HIGH;
bool currState_E = HIGH;

bool prevState_B = HIGH;
bool prevState_D = HIGH;
bool prevState_U = HIGH;
bool prevState_E = HIGH;

unsigned long prevTime_B = 0;
unsigned long prevTime_D = 0;
unsigned long prevTime_U = 0;
unsigned long prevTime_E = 0;
unsigned long prevTime_moisture = 0; //untuk moisture sensor
unsigned long prevTime_pump = 0;     //untuk pompa
unsigned long prevTime_rtc = 0;      // untuk rtc

unsigned long waitTime_B = 50;
unsigned long waitTime_D = 50;
unsigned long waitTime_U = 50;
unsigned long waitTime_E = 50;
unsigned long waitTime_moisture = 1000; //baca sensor tiap 1 detik
unsigned long waitTime_pump = 50;       //interval agar pompa dapat bekerja secara independen
unsigned long waitTime_rtc = 1000;      //baca jam tiap satu detik

// ================================ Custom Character ============================================
byte customThermometer[] = {
    0x04,
    0x0A,
    0x0A,
    0x0A,
    0x11,
    0x1F,
    0x1F,
    0x0E};

byte customDegree[] = {
    0x04,
    0x0A,
    0x0A,
    0x04,
    0x00,
    0x00,
    0x00,
    0x00};

byte customWater[] = {
    0x00,
    0x04,
    0x0A,
    0x11,
    0x11,
    0x11,
    0x0E,
    0x00};

byte customArrow[] = {
    0x00,
    0x04,
    0x02,
    0x1F,
    0x02,
    0x04,
    0x00,
    0x00};

byte customArrow2[] = {
    0x00,
    0x04,
    0x08,
    0x1F,
    0x08,
    0x04,
    0x00,
    0x00};

byte customBar1[] = {
    0x00,
    0x0F,
    0x10,
    0x17,
    0x17,
    0x10,
    0x0F,
    0x00};

byte customBar2[] = {
    0x00,
    0x1F,
    0x00,
    0x1F,
    0x1F,
    0x00,
    0x1F,
    0x00};

byte customBar3[] = {
    0x00,
    0x1E,
    0x01,
    0x1D,
    0x1D,
    0x01,
    0x1E,
    0x00};

// ================================== Setup ===================================================

void setup()
{
  lcd.init();
  lcd.backlight();
  Serial.begin(9600);

  pinMode(button_B, INPUT_PULLUP);
  pinMode(button_D, INPUT_PULLUP);
  pinMode(button_U, INPUT_PULLUP);
  pinMode(button_E, INPUT_PULLUP);
  pinMode(soil, INPUT);
  pinMode(hpl, OUTPUT);
  pinMode(pump, OUTPUT);

  //setup awal pompa
  digitalWrite(pump, HIGH);
  lcd.setCursor(3, 1);
  lcd.print("Tugas Besar");
  delay(1000);
  lcd.setCursor(6, 2);
  lcd.print("Agustinus Y");
  delay(2000);
  lcd.clear();
  //settingan awal
  Wire.begin();
  rtc.begin();
  showHomeScreen();
  //membuat custom character
  lcd.createChar(0, customThermometer);
  lcd.createChar(1, customDegree);
  lcd.createChar(2, customWater);
  lcd.createChar(3, customArrow);
  lcd.createChar(4, customBar1);
  lcd.createChar(5, customBar2);
  lcd.createChar(6, customBar3);
  lcd.createChar(7, customArrow2);
  lcd.home();
}

// =================================== Loop ======================================
void loop()
{
  unsigned long eventTimer = millis(); //waktu untuk menjalankan sensor
  // You can do other things here below
  checkButton();
  moistureReading(eventTimer);
  time(eventTimer);
  //manipulasi relay active low menjadi active high
  if (activeHigh == false)
  {
    digitalWrite(pump, HIGH);
  }
  else
  {
    digitalWrite(pump, LOW);
  }
  pumpExe();
  // assign variabel awal di time setting
  if (!timeSetting)
  {
    h2 = hour;
    m2 = minute;
    s2 = second;
  }
}

//================================== Button Debouncing ==============================
// Debouncing agar noise-noise dari pushbutton (ketika pemencetan tidak sempurna)
// tidak dianggap sebagai input
// Sekaligus masuk ke submenu-submenu
void checkButton()
{
  // Button Debouncing
  bool currRead_B = digitalRead(button_B);
  bool currRead_D = digitalRead(button_D);
  bool currRead_U = digitalRead(button_U);
  bool currRead_E = digitalRead(button_E);

  if (currRead_B != prevState_B)
  {
    prevTime_B = millis();
  }
  if (currRead_D != prevState_D)
  {
    prevTime_D = millis();
  }
  if (currRead_U != prevState_U)
  {
    prevTime_U = millis();
  }
  if (currRead_E != prevState_E)
  {
    prevTime_E = millis();
  }

  if ((millis() - prevTime_B) > waitTime_B)
  {
    if (currRead_B != currState_B)
    {
      currState_B = currRead_B;
      if (currState_B == LOW)
      {
        buttonPressed = 'B';
      }
    }
  }
  else
    buttonPressed = '0';
  if ((millis() - prevTime_D) > waitTime_D)
  {
    if (currRead_D != currState_D)
    {
      currState_D = currRead_D;
      if (currState_D == LOW)
      {
        buttonPressed = 'D';
      }
    }
  }
  else
    buttonPressed = '0';
  if ((millis() - prevTime_U) > waitTime_U)
  {
    if (currRead_U != currState_U)
    {
      currState_U = currRead_U;
      if (currState_U == LOW)
      {
        buttonPressed = 'U';
      }
      else
      {
        //buttonPressed = '0';
      }
    }
  }
  else
    buttonPressed = '0';
  if ((millis() - prevTime_E) > waitTime_E)
  {
    if (currRead_E != currState_E)
    {
      currState_E = currRead_E;
      if (currState_E == LOW)
      {
        buttonPressed = 'E';
      }
    }
  }
  else
    buttonPressed = '0';

  prevState_B = currRead_B;
  prevState_D = currRead_D;
  prevState_U = currRead_U;
  prevState_E = currRead_E;

  processButton(buttonPressed);
}

// ======================================= MENU =====================================
//Proses pemilihan menu, 0 = menu awal - 2 = submenu
void processButton(char buttonPressed)
{

  switch (menuLevel)
  {
  case 0: // Level 0
    if (!manLed)
    {
      automaticMode(hour, minute);
    }
    showHomeScreen(); //tampilkan tampilan awal yang diloop untuk update
    switch (buttonPressed)
    {
    case 'E': // Enter
      menuLevel = 1;
      menu = 1;
      updateMenu();
      delay(DEFAULT_DELAY);
      break;
    case 'U': // Up
      break;
    case 'D': // Down
      break;
    case 'B': // Back
      break;
    default:
      break;
    }
    break;
  case 1: // Level 1, main menu
    switch (buttonPressed)
    {
    case 'E': // Enter
      updateSub();
      menuLevel = 2; // go to sub menu
      updateSub();
      delay(DEFAULT_DELAY);
      break;
    case 'U': // Up
      menu--; //Menunya jadi naik
      Serial.println(menu);
      updateMenu();
      delay(DEFAULT_DELAY);
      break;
    case 'D': // Down
      menu++; //Menunya jadi turun
      Serial.println(menu);
      updateMenu();
      delay(DEFAULT_DELAY);
      break;
    case 'B':        // Back
      menuLevel = 0; // Kembali ke tampilan awal
      lcd.clear();
      showHomeScreen();
      delay(DEFAULT_DELAY);
      break;
    default:
      break;
    }
    break;
  case 2: // Level 2, sub menu
    switch (buttonPressed)
    {
    case 'E':
      if (menu == 1)
      {
        lcd.setCursor(0, 2);
        lcd.print("Applying changes...");
        delay(DEFAULT_DELAY * 2);
        int val = ledMapping(ledMode);
        appliedMode = ledMode;
        if (ledMode == 0)
        {
          manLed = false;
          automaticMode(hour, minute); //eksekusi mode otomatis
        }
        else
        {
          manLed = true;
          analogWrite(hpl, val);
        }
        updateSub();
      }
      else if (menu == 2)
      {
        //setting waktu manual
        if (timeSet >= 2)
        {
          lcd.setCursor(0, 3);
          DateTime dt(year, month, date, h2, m2, s2, day);
          rtc.setDateTime(dt);
          lcd.print("Applying changes...");
          delay(DEFAULT_DELAY);
          automaticMode(h2, m2);
          timeSet = 0;
        }
        else
        {
          timeSet++;
          lcd.setCursor(timeSet + 6, 3);
        }
        updateSub();
      }
      else if (menu = 3)
      {
        //testpunp
        testpump();
        updateSub();
      }
      delay(DEFAULT_DELAY);
      break;
    case 'U': // Up
      if (menu == 1)
      {
        if (ledMode > 4)
        {
          ledMode = 0;
        }
        else
        {
          ledMode++;
        }
      }
      else if (menu == 2)
      {
        switch (timeSet)
        { //pengaturan settingan jam
        case 0:
          if (h2 > 22)
          {
            h2 = 0;
          }
          else
          {
            h2++;
          }
          break;
        case 1:
          if (m2 > 58)
          {
            m2 = 0;
            if (h2 == 23)
            {
              h2 = 0;
            }
            else
            {
              h2 += 1;
            }
          }
          else
          {
            m2++;
          }
          break;
        case 2:
          if (s2 > 58)
          {
            s2 = 0;
            if (m2 == 59)
            {
              if (h2 == 23)
              {
                h2 = 0;
                m2 = 0;
              }
              else
              {
                h2 += 1;
                m2 = 0;
              }
            }
            else
            {
              m2 += 1;
            }
          }
          else
          {
            s2 += 1;
          }
          break;
        }
      }
      updateSub();
      delay(DEFAULT_DELAY);
      break;
    case 'D': // Down
      if (menu == 1)
      {
        if (ledMode == 0)
        {
          ledMode = 5;
        }
        else
        {
          ledMode--;
        }
      }
      else if (menu == 2)
      {
        switch (timeSet)
        { //pengaturan settingan jam
        case 0:
          if (h2 < 1)
          {
            h2 = 23;
          }
          else
          {
            h2--;
          }
          break;
        case 1:
          if (m2 < 1)
          {
            m2 = 59;
            if (h2 == 0)
            {
              h2 = 23;
            }
            else
            {
              h2--;
            }
          }
          else
          {
            m2--;
          }
          break;
        case 2:
          if (s2 < 1)
          {
            s2 = 59;
            if (m2 == 00)
            {
              m2 = 59;
              if (h2 == 00)
              {
                h2 = 23;
              }
              else
              {
                h2 -= 1;
              }
            }
            else
            {
              m2 -= 1;
            }
          }
          else
          {
            s2 -= 1;
          }
          break;
        }
      }
      updateSub();
      delay(DEFAULT_DELAY);
      break;
    case 'B': // L
      timeSetting = false;
      ledMode = appliedMode; //reset pembacaan ledMode
      menuLevel = 1; // go back to main menu
      updateMenu();
      delay(DEFAULT_DELAY);
      break;
    default:
      break;
    }
    break;
  case 3: // Level 3, sub menu of sub menu
    break;
  default:
    break;
  }
}

void updateMenu()
{ //update menu
  switch (menu)
  {
  case 0:
    menu = 1;
    break;
  case 1:
    lcd.clear();
    lcd.setCursor(0, 3);
    lcd.print("B");
    lcd.write(7);
    lcd.setCursor(18, 3);
    lcd.write(3);
    lcd.print("R");
    lcd.setCursor(6, 0);
    lcd.print("Settings");
    lcd.setCursor(4, 2);
    lcd.print("->LED: ");
    if (appliedMode == 0)
    {
      lcd.print("Df");
    }
    else
    {
      lcd.print(appliedMode);
    }

    lcd.setCursor(4, 3);
    lcd.print("  Waktu ");
    break;
  case 2:
    lcd.clear();
    lcd.setCursor(0, 3);
    lcd.print("B");
    lcd.write(7);
    lcd.setCursor(18, 3);
    lcd.write(3);
    lcd.print("R");
    lcd.setCursor(6, 0);
    lcd.print("Settings");
    lcd.setCursor(4, 2);
    lcd.print("  LED: ");
    if (appliedMode == 0)
    {
      lcd.print("Df");
    }
    else
    {
      lcd.print(appliedMode);
    }
    lcd.setCursor(4, 3);
    lcd.print("->Waktu ");
    break;
  case 3:
    lcd.clear();
    lcd.setCursor(0, 3);
    lcd.print("B");
    lcd.write(7);
    lcd.setCursor(18, 3);
    lcd.write(3);
    lcd.print("R");
    lcd.setCursor(6, 0);
    lcd.print("Settings");
    lcd.setCursor(4, 2);
    lcd.print("  Waktu ");
    lcd.setCursor(4, 3);
    lcd.print("->Pump Test");
    break;
  case 4:
    menu = 3;
    break;
  }
}

void updateSub()
{ //update submenu
  switch (menu)
  {
  case 0:
    break;
  case 1:
    lcd.clear();
    lcd.print("LED:");
    if (ledMode == 0)
    {
      lcd.print("Df");
    }
    else
    {
      lcd.print(ledMode);
    }
    break;
  case 2:
    timeSetting = true;
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("Time Settings");
    lcd.setCursor(6, 2);
    printTime(h2, m2, s2, true);
    break;
  case 3:
    lcd.clear();
    lcd.setCursor(4, 0);
    lcd.print("Pump Testing");
    lcd.setCursor(6,2);
    lcd.print("N");
    lcd.write(7);
    lcd.print("   ");
    lcd.write(3);
    lcd.print("Y");
  }
}

void showHomeScreen()
{
  // lcd.clear();
  lcd.setCursor(3, 0);
  lcd.print("Sistem Monitor");
  lcd.setCursor(15, 1);
  // lcd.print(day);
  printTime(hour, minute, second, false);
  lcd.setCursor(0, 1);
  lcd.write(0);
  lcd.print(" ");
  lcd.print(temp);
  lcd.write(1);
  lcd.print("C");
  lcd.setCursor(0, 2);
  lcd.write(2);
  lcd.print(" ");
  if (moist < 10)
  {
    lcd.setCursor(2, 2);
    lcd.print("0");
    lcd.print(abs(moist));
  }
  else
  {
    lcd.setCursor(2, 2);
    lcd.print(moist);
  }
  lcd.setCursor(5, 2);
  lcd.print("%");
  lcd.setCursor(18, 3);
  lcd.write(3);
  lcd.print("R");
  //   lcd.setCursor(0,1);
  //   lcd.println("  Agustinus Yudhistira  ");
}

//======================================= Fungsi Untuk Utilitas(Sensor, Jam, dkk) =============================================
int ledMapping(int mode)
{
  int mapped = map(mode, 1, 5, 0, 255);
  return mapped;
}

//RTC module setting nyala dari jam 6 pagi sampe jam 6 sore
void automaticMode(int hour, int minute)
{
  if ((hour >= 6 && minute >= 0) && (hour < 18))
  {
    analogWrite(hpl, 255);
    appliedMode = 0;
    ledMode = 0;
    // Serial.println("Lampu otomatis nyala");
  }
  else if ((hour >= 18 && minute >= 00))
  {
    // Serial.println("Lampu otomatis mati");
    analogWrite(hpl, 0);
    appliedMode = 0;
    ledMode = 0;
  }
}

//Sensor moisture tanah + temperature
void moistureReading(unsigned long timeEvent)
{
  if (timeEvent - prevTime_moisture >= waitTime_moisture)
  {
    soilValue = analogRead(soil);
    moist = map(soilValue, 650, 310, 5, 95);
    Serial.println(soilValue);
    temp = rtc.getTemperature();
    // Serial.println(temp);

    prevTime_moisture = timeEvent;
  }
}

//pompa
void pumpExe()
{
  if (automaticPump)
  {
    if (soilValue > 475)
    {
      // Serial.println("Pump is ON");
      activeHigh = true;
    }
    else
    {
      // Serial.println("Wet, Pump is OFF");
      activeHigh = false;
    }
  }
}

//testpump
void testpump()
{
  digitalWrite(pump, LOW);
  lcd.clear();
  lcd.setCursor(7, 1);
  lcd.print("Pumping");
  lcd.setCursor(8, 3);
  lcd.write(4);
  delay(1000);
  for (int i = 0; i < 3; i++)
  {
    lcd.write(5);
    delay(1000);
  }
  lcd.write(6);
  delay(1000);
}

//Automatic time reading (RTC)
void time(unsigned long timeEvent)
{
  if (timeEvent - prevTime_rtc >= waitTime_rtc)
  {
    DateTime now = rtc.now();
    year = now.year();
    month = now.month();
    date = now.date();
    day = weekDay[now.dayOfWeek()];
    hour = now.hour();
    minute = now.minute();
    second = now.second();
    prevTime_rtc = timeEvent;
  }
}

//print jam ke lcd
void printTime(int hour, int minute, int second, bool check)
{
  if (hour < 10)
  {
    lcd.print(0);
    lcd.print(hour);
  }
  else
  {
    lcd.print(hour);
  }
  lcd.print(":");
  if (minute < 10)
  {
    lcd.print(0);
    lcd.print(minute);
  }
  else
  {
    lcd.print(minute);
  }
  if (check)
  {
    lcd.print(":");
    if (second < 10)
    {
      lcd.print(0);
      lcd.print(second);
    }
    else
    {
      lcd.print(second);
    }
  }
}
