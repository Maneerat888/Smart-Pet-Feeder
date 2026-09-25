#include <Arduino.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>


// PIN CONFIGURATION


// LED
const int LED_GREEN  = 25;   // สีเขียว = พร้อมใช้งาน
const int LED_YELLOW = 26;   // สีเหลือง = กำลังให้อาหาร
const int LED_RED    = 27;   // สีแดง = อาหารหมด

// Input
const int BUTTON_PIN = 13;   // ปุ่มให้อาหารทันที
const int POT_PIN    = 34;   // Potentiometer


// ULTRASONIC #1 : FOOD LEVEL


const int TRIG_PIN = 5;
const int ECHO_PIN = 18;



// ULTRASONIC #2 : CAT DETECTION


const int CAT_TRIG_PIN = 16;
const int CAT_ECHO_PIN = 17;

// ระยะที่ถือว่าพบแมว
const float CAT_DETECT_DISTANCE = 11.0;

// แมวต้องเดินออกเกิน 11 cm
// จึงจะเริ่มนับรอบใหม่ได้
const float CAT_LEAVE_DISTANCE = 11.0;

// ต้องอยู่ในระยะ 11 cm เป็นเวลา 3 วินาที
const unsigned long CAT_EAT_TIME = 3000;

// จำนวนครั้งที่แมวกิน
// ค่านี้จะสะสมตลอดเวลาที่ ESP32 เปิดอยู่
int catEatCount = 0;

// เวลาเริ่มตรวจพบแมว
unsigned long catStartTime = 0;

// สถานะว่าตอนนี้มีแมวอยู่หรือไม่
bool catDetected = false;

// สถานะว่ารอบนี้นับไปแล้วหรือยัง
bool catCounted = false;


// =====================================================
// SERVO
// =====================================================

const int SERVO_PIN = 19;

Servo feederServo;

// ตำแหน่ง Servo
const int SERVO_CLOSE = 0;
const int SERVO_OPEN  = 40;



// OLED I2C

const int OLED_SDA = 21;
const int OLED_SCL = 22;

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET -1

Adafruit_SSD1306 display(
    SCREEN_WIDTH,
    SCREEN_HEIGHT,
    &Wire,
    OLED_RESET
);



// FUNCTION PROTOTYPE


void showDisplay(String status, float distance);



// FEEDING SETTINGS


// เวลาเปิด Servo แต่ละระดับ
const int LOW_TIME    = 200;
const int MEDIUM_TIME = 400;
const int HIGH_TIME   = 600;

// ระดับอาหารที่เลือก
String selectedAmount = "MEDIUM";

// เวลาที่ Servo เปิด
int feedTime = MEDIUM_TIME;

// จำนวนครั้งที่ให้อาหารวันนี้
int feedCount = 0;

// FOOD LEVEL SETTINGS

// ระยะจาก Sensor ถึงอาหาร


const float FOOD_FULL_DISTANCE  = 4.0;
const float FOOD_LOW_DISTANCE   = 13.0;
const float FOOD_EMPTY_DISTANCE = 15.0;


// FUNCTION: READ POTENTIOMETER

void readAmount()
{
    int adcValue = analogRead(POT_PIN);

    // LOW
    if (adcValue < 1365)
    {
        selectedAmount = "LOW";
        feedTime = LOW_TIME;
    }

    // MEDIUM
    else if (adcValue < 2730)
    {
        selectedAmount = "MEDIUM";
        feedTime = MEDIUM_TIME;
    }

    // HIGH
    else
    {
        selectedAmount = "HIGH";
        feedTime = HIGH_TIME;
    }
}


// FUNCTION: READ FOOD ULTRASONIC

float readDistance()
{
    digitalWrite(TRIG_PIN, LOW);

    delayMicroseconds(2);

    digitalWrite(TRIG_PIN, HIGH);

    delayMicroseconds(10);

    digitalWrite(TRIG_PIN, LOW);

    long duration = pulseIn(
        ECHO_PIN,
        HIGH,
        30000
    );

    // ถ้าไม่ได้รับสัญญาณ
    if (duration == 0)
    {
        return 999;
    }

    // คำนวณระยะเป็น cm
    float distance =
        duration * 0.0343 / 2;

    return distance;
}


// FUNCTION: READ CAT ULTRASONIC

float readCatDistance()
{
    digitalWrite(
        CAT_TRIG_PIN,
        LOW
    );

    delayMicroseconds(2);

    digitalWrite(
        CAT_TRIG_PIN,
        HIGH
    );

    delayMicroseconds(10);

    digitalWrite(
        CAT_TRIG_PIN,
        LOW
    );

    long duration = pulseIn(
        CAT_ECHO_PIN,
        HIGH,
        30000
    );

    // ถ้าไม่ได้รับสัญญาณ
    if (duration == 0)
    {
        return 999;
    }

    // คำนวณระยะเป็น cm
    float distance =
        duration * 0.0343 / 2;

    return distance;
}

// FUNCTION: CHECK CAT

void checkCat()
{
    // อ่านระยะจาก Sensor แมว
    float catDistance =
        readCatDistance();


    // CASE 1 : พบแมว
 

    if (catDistance <= CAT_DETECT_DISTANCE)
    {
        // เพิ่งพบแมว
   
        if (!catDetected)
        {
            catDetected = true;

            // ยังไม่นับ
            catCounted = false;

            // เริ่มจับเวลา
            catStartTime = millis();

            Serial.println();
            Serial.println("------------------------");
            Serial.println("CAT DETECTED");
            Serial.println("START COUNTING 3 SECONDS");
        }


        // แมวยังอยู่ และยังไม่ถูกนับ
      

        if (!catCounted)
        {
            unsigned long currentTime =
                millis();

            // อยู่ครบ 3 วินาที
            if (
                currentTime - catStartTime
                >= CAT_EAT_TIME
            )
            {
               
                // เพิ่มจำนวนครั้งที่แมวกิน
               
                catEatCount++;

                // ป้องกันการนับซ้ำ
                catCounted = true;

                Serial.println(
                    "CAT EAT DETECTED!"
                );

                Serial.print(
                    "CAT EAT TODAY: "
                );

                Serial.println(
                    catEatCount
                );

                // อ่านระดับอาหาร
                float foodDistance =
                    readDistance();

                // แสดงผลบน OLED
                showDisplay(
                    "CAT EAT!",
                    foodDistance
                );
            }
        }
    }

    // CASE 2 : แมวเดินออก

    else if (
        catDistance > CAT_LEAVE_DISTANCE
    )
    {
        // ถ้าเคยพบแมว
        if (catDetected)
        {
            Serial.println(
                "CAT LEFT"
            );

            Serial.println(
                "READY FOR NEXT VISIT"
            );

            catDetected = false;

            catCounted = false;

            catStartTime = 0;
        }
    }
}



// FUNCTION: CHECK FOOD EMPTY

bool isFoodEmpty(float distance)
{


    if (
        distance >= FOOD_EMPTY_DISTANCE
    )
    {
        return true;
    }

    return false;
}


// FUNCTION: CHECK FOOD LOW


bool isFoodLow(float distance)
{

    if (
        distance >= FOOD_LOW_DISTANCE &&
        distance < FOOD_EMPTY_DISTANCE
    )
    {
        return true;
    }

    return false;
}

// FUNCTION: CALCULATE FOOD PERCENT

int calculateFoodPercent(
    float distance
)
{
    // 2 cm = 100%
    // 13 cm = 0%

    int percent = map(
        (int)distance,
        (int)FOOD_FULL_DISTANCE,
        (int)FOOD_EMPTY_DISTANCE,
        100,
        0
    );

    // จำกัดค่า 0-100
    percent = constrain(
        percent,
        0,
        100
    );

    return percent;
}
// FUNCTION: OLED DISPLAY

void showDisplay(
    String status,
    float distance
)
{
    // คำนวณเปอร์เซ็นต์อาหาร
    int foodPercent =
        calculateFoodPercent(
            distance
        );

    // ล้างหน้าจอ
    display.clearDisplay();

    // ตั้งค่าข้อความ
    display.setTextSize(1);

    display.setTextColor(
        SSD1306_WHITE
    );
    // ชื่อระบบ
  

    display.setCursor(0, 0);

    display.println(
        "SMART PET FEEDER"
    );

    display.println(
        "----------------"
    );

    // ปริมาณอาหารที่จะปล่อย

    display.print(
        "AMOUNT : "
    );

    display.println(
        selectedAmount
    );

    // อาหารในถัง

    display.print(
        "FOOD   : "
    );

    display.print(
        foodPercent
    );

    display.println(
        "%"
    );

    // จำนวนครั้งที่เครื่องให้อาหาร
  
    display.print(
        "TODAY  : "
    );

    display.print(
        feedCount
    );

    display.println(
        " TIMES"
    );

    // จำนวนครั้งที่แมวกิน
   

    display.print(
        "CAT EAT: "
    );

    display.print(
        catEatCount
    );

    display.println(
        " TIMES"
    );

    // สถานะ


    display.print(
        "STATUS : "
    );

    display.println(
        status
    );


    // แสดงผล
    display.display();
}

// FUNCTION: READY LED


void setReadyLED()
{
    digitalWrite(
        LED_GREEN,
        HIGH
    );

    digitalWrite(
        LED_YELLOW,
        LOW
    );

    digitalWrite(
        LED_RED,
        LOW
    );
}

// FUNCTION: FEEDING LED


void setFeedingLED()
{
    digitalWrite(
        LED_GREEN,
        LOW
    );

    digitalWrite(
        LED_YELLOW,
        HIGH
    );

    digitalWrite(
        LED_RED,
        LOW
    );
}

// FUNCTION: EMPTY LED

void setEmptyLED()
{
    digitalWrite(
        LED_GREEN,
        LOW
    );

    digitalWrite(
        LED_YELLOW,
        LOW
    );

    digitalWrite(
        LED_RED,
        HIGH
    );
}


// FUNCTION: FEED PET

void feedPet()
{
    // อ่านระดับอาหาร
    float distance =
        readDistance();

    // อาหารหมด
 

    if (
        isFoodEmpty(distance)
    )
    {
        setEmptyLED();

        showDisplay(
            "FOOD EMPTY",
            distance
        );

        Serial.println(
            "Cannot feed!"
        );

        Serial.println(
            "Food is empty."
        );

        return;
    }


    // เริ่มให้อาหาร

    Serial.println();
    Serial.println(
        "------------------------"
    );

    Serial.println(
        "FEEDING START"
    );

    Serial.print(
        "Amount: "
    );

    Serial.println(
        selectedAmount
    );


    // เปิดไฟเหลือง
    setFeedingLED();


    // OLED
    showDisplay(
        "FEEDING",
        distance
    );


    // เปิดช่องอาหาร
    feederServo.write(
        SERVO_OPEN
    );


    // รอตามปริมาณ
    delay(
        feedTime
    );


    // ปิดช่องอาหาร
    feederServo.write(
        SERVO_CLOSE
    );


    // ปิดไฟเหลือง
    digitalWrite(
        LED_YELLOW,
        LOW
    );


    // เพิ่มจำนวนครั้งให้อาหาร
    feedCount++;


    Serial.println(
        "FEEDING FINISHED"
    );

    Serial.print(
        "Today: "
    );

    Serial.print(
        feedCount
    );

    Serial.println(
        " times"
    );

    // ตรวจสอบอาหารหลังให้อาหาร

    distance =
        readDistance();


    // อาหารหมด
    if (
        isFoodEmpty(distance)
    )
    {
        setEmptyLED();

        showDisplay(
            "FOOD EMPTY",
            distance
        );

        Serial.println(
            "WARNING: FOOD EMPTY!"
        );
    }


    // อาหารน้อย
    else if (
        isFoodLow(distance)
    )
    {
        setReadyLED();

        showDisplay(
            "FOOD LOW!",
            distance
        );

        Serial.println(
            "WARNING: FOOD LOW!"
        );
    }


    // อาหารเพียงพอ
    else
    {
        setReadyLED();

        showDisplay(
            "READY",
            distance
        );
    }

    Serial.println(
        "------------------------"
    );
}
// FUNCTION: BUTTON

void checkButton()
{
    int buttonState =
        digitalRead(
            BUTTON_PIN
        );


    // ปุ่มถูกกด
    if (
        buttonState == LOW
    )
    {
        // Debounce
        delay(50);


        if (
            digitalRead(
                BUTTON_PIN
            ) == LOW
        )
        {
            // ให้อาหารทันที
            feedPet();


            // รอจนปล่อยปุ่ม
            while (
                digitalRead(
                    BUTTON_PIN
                ) == LOW
            )
            {
                delay(10);
            }
        }
    }
}

// FUNCTION: AUTOMATIC FEEDING
void automaticFeeding()
{
    // เก็บเวลาการให้อาหารครั้งล่าสุด
    static unsigned long previousFeedTime = 0;

    unsigned long currentTime =
        millis();


    // ให้อาหารทุก 20 วินาที
    if (
        currentTime - previousFeedTime
        >= 20000
    )
    {
        previousFeedTime =
            currentTime;


        Serial.println();
        Serial.println(
            "AUTO FEEDING"
        );

        Serial.println(
            "EVERY 20 SECONDS"
        );


        feedPet();
    }
}

// SETUP

void setup()
{
    // SERIAL

    Serial.begin(
        115200
    );

    // LED

    pinMode(
        LED_GREEN,
        OUTPUT
    );

    pinMode(
        LED_YELLOW,
        OUTPUT
    );

    pinMode(
        LED_RED,
        OUTPUT
    );

    // BUTTON

    pinMode(
        BUTTON_PIN,
        INPUT_PULLUP
    );
    // FOOD ULTRASONIC

    pinMode(
        TRIG_PIN,
        OUTPUT
    );

    pinMode(
        ECHO_PIN,
        INPUT
    );
    // CAT ULTRASONIC

    pinMode(
        CAT_TRIG_PIN,
        OUTPUT
    );

    pinMode(
        CAT_ECHO_PIN,
        INPUT
    );

    // POTENTIOMETER

    pinMode(
        POT_PIN,
        INPUT
    );
    // OLED

    Wire.begin(
        OLED_SDA,
        OLED_SCL
    );


    if (
        !display.begin(
            SSD1306_SWITCHCAPVCC,
            0x3C
        )
    )
    {
        Serial.println(
            "OLED ERROR!"
        );

        while (true)
        {
            delay(1000);
        }
    }

    // SERVO
    feederServo.attach(
        SERVO_PIN
    );


    // Servo ปิด
    feederServo.write(
        SERVO_CLOSE
    );

    // INITIAL SETTINGS
    readAmount();


    // อ่านระดับอาหาร
    float distance =
        readDistance();

    // INITIAL FOOD STATUS

    if (
        isFoodEmpty(distance)
    )
    {
        setEmptyLED();

        showDisplay(
            "FOOD EMPTY",
            distance
        );
    }

    else if (
        isFoodLow(distance)
    )
    {
        setReadyLED();

        showDisplay(
            "FOOD LOW!",
            distance
        );
    }

    else
    {
        setReadyLED();

        showDisplay(
            "READY",
            distance
        );
    }

    // START MESSAGE


    Serial.println(
        "========================"
    );

    Serial.println(
        "SMART PET FEEDER"
    );

    Serial.println(
        "SYSTEM READY"
    );

    Serial.println(
        "AUTO FEED : 20 SECONDS"
    );

    Serial.println(
        "CAT DISTANCE : 11 CM"
    );

    Serial.println(
        "CAT TIME : 3 SECONDS"
    );

    Serial.println(
        "FOOD FULL : 2 CM"
    );

    Serial.println(
        "FOOD LOW : 10 CM"
    );

    Serial.println(
        "FOOD EMPTY : 13 CM"
    );

    Serial.println(
        "========================"
    );
}

// LOOP


void loop()
{
    // อ่านปริมาณอาหารที่เลือก

    readAmount();
    // MANUAL FEED
    checkButton();

    // AUTOMATIC FEED

    automaticFeeding();
    // CAT DETECTION

    checkCat();
    // READ FOOD LEVEL

    float distance =
        readDistance();
    // FOOD STATUS

    if (
        isFoodEmpty(distance)
    )
    {
        setEmptyLED();

        showDisplay(
            "FOOD EMPTY",
            distance
        );
    }

    else if (
        isFoodLow(distance)
    )
    {
        setReadyLED();

        showDisplay(
            "FOOD LOW!",
            distance
        );
    }

    else
    {
        // ถ้าไม่ได้กำลังให้อาหาร
        if (
            digitalRead(
                LED_YELLOW
            ) == LOW
        )
        {
            setReadyLED();

            showDisplay(
                "READY",
                distance
            );
        }
    }


    // รอเล็กน้อย
    delay(200);
}
