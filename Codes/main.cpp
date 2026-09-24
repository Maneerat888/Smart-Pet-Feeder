#include <Arduino.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// PIN CONFIGURATION


// LED
const int LED_GREEN  = 25;   // สีเขียว = เครื่องพร้อมใช้งาน
const int LED_YELLOW = 26;   // สีเหลือง = กำลังปล่อยอาหาร
const int LED_RED    = 27;   // สีแดง = อาหารหมด

// Input
const int BUTTON_PIN = 13;   // ปุ่มให้อาหารทันที
const int POT_PIN    = 34;   // Potentiometer

// Ultrasonic #1 : FOOD LEVEL

const int TRIG_PIN = 5;
const int ECHO_PIN = 18;


// Ultrasonic #2 : CAT DETECTION


const int CAT_TRIG_PIN = 16;
const int CAT_ECHO_PIN = 17;

// ระยะที่ถือว่าตรวจพบแมว
const float CAT_DETECT_DISTANCE = 30.0;

// ระยะที่ถือว่าแมวเดินออกไปแล้ว
// ใช้ค่ามากกว่า CAT_DETECT_DISTANCE
// เพื่อป้องกันการนับซ้ำจากค่าระยะที่แกว่ง
const float CAT_LEAVE_DISTANCE = 35.0;

// ต้องยืนอยู่ 15 วินาที จึงนับ 1 ครั้ง
const unsigned long CAT_EAT_TIME = 15000;

// ตัวแปรสำหรับนับการมากิน
int catEatCount = 0;

// เวลาเริ่มต้นที่ตรวจพบแมว
unsigned long catStartTime = 0;

// สถานะว่าตอนนี้พบแมวหรือไม่
bool catDetected = false;

// สถานะว่านับครั้งนี้ไปแล้วหรือยัง
bool catCounted = false;


// Servo


const int SERVO_PIN = 19;

// OLED I2C

const int OLED_SDA = 21;
const int OLED_SCL = 22;


// OLED CONFIGURATION


#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET
);

// SERVO


Servo feederServo;

// ตำแหน่ง Servo
const int SERVO_CLOSE = -2;
const int SERVO_OPEN  = 45;


// FEEDING SETTINGS


// เวลาที่ Servo เปิดในแต่ละระดับ
const int LOW_TIME    = 500;
const int MEDIUM_TIME = 1000;
const int HIGH_TIME   = 1500;

// ระดับอาหารที่เลือก
String selectedAmount = "MEDIUM";

// เวลาปล่อยอาหาร
int feedTime = MEDIUM_TIME;

// จำนวนครั้งที่ให้อาหารวันนี้
int feedCount = 0;

// FOOD LEVEL SETTINGS


// ระยะ Sensor ถึงอาหาร
// 5 cm  = อาหารเต็ม
// 12 cm = อาหารใกล้หมด
// 16 cm = อาหารหมด

const float FOOD_FULL_DISTANCE  = 5.0;
const float FOOD_LOW_DISTANCE   = 12.0;
const float FOOD_EMPTY_DISTANCE = 16.0;


// FUNCTION: READ POTENTIOMETER


void readAmount()
{
    // อ่านค่า Analog จาก Potentiometer
    int adcValue = analogRead(POT_PIN);

    // แบ่งค่า ADC เป็น 3 ระดับ
    if (adcValue < 1365)
    {
        selectedAmount = "LOW";
        feedTime = LOW_TIME;
    }
    else if (adcValue < 2730)
    {
        selectedAmount = "MEDIUM";
        feedTime = MEDIUM_TIME;
    }
    else
    {
        selectedAmount = "HIGH";
        feedTime = HIGH_TIME;
    }
}


// FUNCTION: READ ULTRASONIC - FOOD


float readDistance()
{
    // ทำให้ TRIG เป็น LOW ก่อน
    digitalWrite(TRIG_PIN, LOW);

    delayMicroseconds(2);

    // ส่ง Pulse 10 microseconds
    digitalWrite(TRIG_PIN, HIGH);

    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    // อ่านเวลาที่ ECHO กลับมา
    long duration = pulseIn(ECHO_PIN, HIGH, 30000);

    // ถ้าไม่พบสัญญาณ
    if (duration == 0)
    {
        return 999;
    }

    // คำนวณระยะทางเป็น cm
    float distance = duration * 0.0343 / 2;

    return distance;
}

// =====================================================
// FUNCTION: READ CAT ULTRASONIC
// =====================================================

float readCatDistance()
{
    // ทำให้ TRIG เป็น LOW ก่อน
    digitalWrite(CAT_TRIG_PIN, LOW);

    delayMicroseconds(2);

    // ส่ง Pulse 10 microseconds
    digitalWrite(CAT_TRIG_PIN, HIGH);

    delayMicroseconds(10);

    digitalWrite(CAT_TRIG_PIN, LOW);

    // อ่านเวลาที่ ECHO กลับมา
    long duration = pulseIn(CAT_ECHO_PIN, HIGH, 30000);

    // ถ้าไม่พบสัญญาณ
    if (duration == 0)
    {
        return 999;
    }

    // คำนวณระยะทางเป็น cm
    float distance = duration * 0.0343 / 2;

    return distance;
}

// FUNCTION: CHECK CAT


void checkCat()
{
    // อ่านระยะจาก Sonic ตัวที่ 2
    float catDistance = readCatDistance();

    // CASE 1 : พบแมว


    if (catDistance <= CAT_DETECT_DISTANCE)
    {
        // ถ้ายังไม่เคยตรวจพบแมว
        if (!catDetected)
        {
            catDetected = true;

            // เริ่มจับเวลา
            catStartTime = millis();

            // ยังไม่นับ
            catCounted = false;

            Serial.println("------------------------");
            Serial.println("CAT DETECTED");
            Serial.println("START COUNTING 15 SECONDS");
        }

   
        // ถ้าแมวยังอยู่ และยังไม่ถูกนับ
       

        if (catDetected && !catCounted)
        {
            unsigned long currentTime = millis();

            // ตรวจว่าครบ 15 วินาทีหรือยัง
            if (currentTime - catStartTime >= CAT_EAT_TIME)
            {
                // นับการกิน +1
                catEatCount++;

                // ป้องกันการนับซ้ำ
                catCounted = true;

                Serial.println("CAT EAT DETECTED!");

                Serial.print("CAT EAT COUNT: ");
                Serial.println(catEatCount);
            }
        }
    }

    // CASE 2 : แมวเดินออก

    else if (catDistance >= CAT_LEAVE_DISTANCE)
    {
        // ถ้าเคยตรวจพบแมว
        if (catDetected)
        {
            Serial.println("CAT LEFT");

            // Reset
            catDetected = false;
            catCounted = false;
            catStartTime = 0;
        }
    }
}


// FUNCTION: CHECK FOOD EMPTY


bool isFoodEmpty(float distance)
{
    // ถ้าระยะ >= 16 cm
    // ถือว่าอาหารหมด

    if (distance >= FOOD_EMPTY_DISTANCE)
    {
        return true;
    }

    return false;
}

// FUNCTION: CHECK FOOD LOW

bool isFoodLow(float distance)
{
    // ถ้าระยะ >= 12 cm
    // แต่ยังไม่ถึง 16 cm
    // ถือว่าอาหารใกล้หมด

    if (distance >= FOOD_LOW_DISTANCE &&
        distance < FOOD_EMPTY_DISTANCE)
    {
        return true;
    }

    return false;
}


// FUNCTION: CALCULATE FOOD LEVEL


int calculateFoodPercent(float distance)
{
    
    // 5 cm = อาหารเต็ม 100%
    // 16 cm = อาหารหมด 0%

    int percent = map(
        (int)distance,
        (int)FOOD_FULL_DISTANCE,
        (int)FOOD_EMPTY_DISTANCE,
        100,
        0
    );

    // จำกัดค่าให้อยู่ระหว่าง 0-100
    percent = constrain(percent, 0, 100);

    return percent;
}


// FUNCTION: OLED DISPLAY


void showDisplay(String status, float distance)
{
    // คำนวณเปอร์เซ็นต์อาหาร
    int foodPercent = calculateFoodPercent(distance);

    // ล้างหน้าจอ
    display.clearDisplay();

    // ตั้งค่าข้อความ
    display.setTextSize(1);
    display.setTextColor(SSD1306_WHITE);

    // ชื่อระบบ
    display.setCursor(0, 0);
    display.println("SMART PET FEEDER");

    display.println("----------------");

    // ระดับอาหารที่จะปล่อย
    display.print("AMOUNT : ");
    display.println(selectedAmount);

    // อาหารในถัง
    display.print("FOOD   : ");
    display.print(foodPercent);
    display.println("%");

    // จำนวนครั้งวันนี้
    display.print("TODAY  : ");
    display.print(feedCount);
    display.println(" TIMES");

    // จำนวนครั้งที่แมวกิน
    display.print("CAT EAT: ");
    display.print(catEatCount);
    display.println(" TIMES");

    // แสดงสถานะ
    display.print("STATUS : ");
    display.println(status);

    // แสดงข้อมูล
    display.display();
}

// FUNCTION: SET LED READY
void setReadyLED()
{
    // สีเขียวติด
    digitalWrite(LED_GREEN, HIGH);

    // สีเหลืองดับ
    digitalWrite(LED_YELLOW, LOW);

    // สีแดงดับ
    digitalWrite(LED_RED, LOW);
}


// FUNCTION: SET LED FEEDING


void setFeedingLED()
{
    // สีเขียวดับ
    digitalWrite(LED_GREEN, LOW);

    // สีเหลืองติด
    digitalWrite(LED_YELLOW, HIGH);

    // สีแดงดับ
    digitalWrite(LED_RED, LOW);
}


// FUNCTION: SET LED EMPTY

void setEmptyLED()
{
    // สีเขียวดับ
    digitalWrite(LED_GREEN, LOW);

    // สีเหลืองดับ
    digitalWrite(LED_YELLOW, LOW);

    // สีแดงติด
    digitalWrite(LED_RED, HIGH);
}


// FUNCTION: FEED PET


void feedPet()
{
    // อ่านระดับอาหารในถัง
    float distance = readDistance();

    // ตรวจสอบว่าอาหารหมดหรือไม่
    if (isFoodEmpty(distance))
    {
        // เปิดไฟแดง
        setEmptyLED();

        // แสดงสถานะบน OLED
        showDisplay("FOOD EMPTY", distance);

        // ส่งข้อความไป Serial Monitor
        Serial.println("Cannot feed!");
        Serial.println("Food is empty.");

        return;
    }

    // เริ่มให้อาหาร
   

    Serial.println("------------------------");
    Serial.println("FEEDING START");

    Serial.print("Amount: ");
    Serial.println(selectedAmount);

    // เปิดไฟเหลือง
    setFeedingLED();

    // แสดง FEEDING บน OLED
    showDisplay("FEEDING", distance);

    // เปิดช่องอาหาร
    feederServo.write(SERVO_OPEN);

    // รอตามระดับปริมาณอาหาร
    delay(feedTime);

    // ปิดช่องอาหาร
    feederServo.write(SERVO_CLOSE);

    // ปิดไฟเหลือง
    digitalWrite(LED_YELLOW, LOW);

    // เพิ่มจำนวนครั้ง
    feedCount++;

    Serial.println("FEEDING FINISHED");

    Serial.print("Today: ");
    Serial.print(feedCount);
    Serial.println(" times");

    // อ่านระดับอาหารอีกครั้ง
    distance = readDistance();

    // ตรวจสอบหลังให้อาหาร
    if (isFoodEmpty(distance))
    {
        // อาหารหมด
        setEmptyLED();

        showDisplay("FOOD EMPTY", distance);

        Serial.println("WARNING: FOOD EMPTY!");
    }
    else if (isFoodLow(distance))
    {
        // อาหารใกล้หมด
        setReadyLED();

        showDisplay("FOOD LOW!", distance);

        Serial.println("WARNING: FOOD LOW!");
    }
    else
    {
        // ยังมีอาหารเพียงพอ
        setReadyLED();

        showDisplay("READY", distance);
    }

    Serial.println("------------------------");
}


// FUNCTION: BUTTON


void checkButton()
{
    // อ่านปุ่ม
    int buttonState = digitalRead(BUTTON_PIN);

    // ปุ่มถูกกด = LOW
    if (buttonState == LOW)
    {
        // Debounce
        delay(50);

        // ตรวจสอบอีกครั้ง
        if (digitalRead(BUTTON_PIN) == LOW)
        {
            // ให้อาหารทันที
            feedPet();

            // รอจนปล่อยปุ่ม
            while (digitalRead(BUTTON_PIN) == LOW)
            {
                delay(10);
            }
        }
    }
}


// FUNCTION: AUTOMATIC FEEDING


void automaticFeeding()
{
    // ใช้ millis() จำลองเวลา

    static unsigned long previousFeedTime = 0;

    unsigned long currentTime = millis();

    // ให้อาหารอัตโนมัติทุก 40 วินาที
    if (currentTime - previousFeedTime >= 40000)
    {
        previousFeedTime = currentTime;

        Serial.println("AUTO FEEDING");
        Serial.println("EVERY 40 SECONDS");

        feedPet();
    }
}

// SETUP

void setup()
{
    // เริ่ม Serial Monitor
    Serial.begin(115200);

    // ตั้งค่า LED เป็น OUTPUT
    pinMode(LED_GREEN, OUTPUT);
    pinMode(LED_YELLOW, OUTPUT);
    pinMode(LED_RED, OUTPUT);

    // ตั้งค่า Button
    pinMode(BUTTON_PIN, INPUT_PULLUP);

    // ตั้งค่า Ultrasonic #1
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);

    // ตั้งค่า Ultrasonic #2 สำหรับแมว
    pinMode(CAT_TRIG_PIN, OUTPUT);
    pinMode(CAT_ECHO_PIN, INPUT);

    // ตั้งค่า Potentiometer
    pinMode(POT_PIN, INPUT);

    // เริ่ม I2C
    Wire.begin(OLED_SDA, OLED_SCL);

    // เริ่ม OLED
    if (!display.begin(
            SSD1306_SWITCHCAPVCC,
            0x3C))
    {
        Serial.println("OLED ERROR!");

        // หยุดโปรแกรม
        while (true)
        {
            delay(1000);
        }
    }

    // เริ่ม Servo
    feederServo.attach(SERVO_PIN);

    // ตั้ง Servo ให้อยู่ตำแหน่งปิด
    feederServo.write(SERVO_CLOSE);

    // อ่าน Potentiometer
    readAmount();

    // อ่านระดับอาหาร
    float distance = readDistance();

    // ตรวจสอบอาหาร
    if (isFoodEmpty(distance))
    {
        setEmptyLED();

        showDisplay("FOOD EMPTY", distance);
    }
    else if (isFoodLow(distance))
    {
        setReadyLED();

        showDisplay("FOOD LOW!", distance);
    }
    else
    {
        setReadyLED();

        showDisplay("READY", distance);
    }

    Serial.println("========================");
    Serial.println("SMART PET FEEDER");
    Serial.println("SYSTEM READY");
    Serial.println("========================");
}

// LOOP


void loop()
{
    // อ่านระดับปริมาณอาหารที่เลือก
    readAmount();

    // ตรวจสอบปุ่ม Manual Feed
    checkButton();

    // ตรวจสอบ Automatic Feed
    automaticFeeding();

    // ตรวจจับสัตว์เลี้ยง
 

    checkCat();

    // อ่านระดับอาหาร
    float distance = readDistance();

    // ตรวจสอบอาหาร
    if (isFoodEmpty(distance))
    {
        // ถ้าอาหารหมด
        setEmptyLED();

        showDisplay("FOOD EMPTY", distance);

        Serial.println("WARNING: FOOD EMPTY!");
    }
    else if (isFoodLow(distance))
    {
        // ถ้าอาหารใกล้หมด
        setReadyLED();

        showDisplay("FOOD LOW!", distance);

        Serial.println("WARNING: FOOD LOW!");
    }
    else
    {
        // ถ้ามีอาหารเพียงพอ
        if (digitalRead(LED_YELLOW) == LOW)
        {
            setReadyLED();

            showDisplay("READY", distance);
        }
    }

    // รอเล็กน้อย
    delay(200);
}