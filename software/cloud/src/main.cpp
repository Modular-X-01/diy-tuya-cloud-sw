// 室内8颗灯    小云
// 室外9颗灯    大云

#define NUM_INDOOR 8
#define NUM_OUTDOOR 9
#define PIN_SHT_RST PB4
#define PIN_SHT_ALERT PB5
#define PIN_WB3_RST PB10
#define PIN_WB3_EN PB11

#include <Arduino.h>
#include "Adafruit_NeoPixel.h"

#define PIN_NEOLED PB1
Adafruit_NeoPixel pixels = Adafruit_NeoPixel(17, PIN_NEOLED, NEO_GRB + NEO_KHZ800);

#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ST7735.h> // Hardware-specific library for ST7735
#include <Adafruit_ST7789.h> // Hardware-specific library for ST7789
#include <SPI.h>
#define TFT_CS PA4
#define TFT_RST PA3 // Or set to -1 and connect to Arduino RESET pin
#define TFT_DC PA2
Adafruit_ST7735 tft = Adafruit_ST7735(TFT_CS, TFT_DC, TFT_RST);

#include "wifi.h"
#include "mcu_api.h"

#include <Wire.h>
#include "ClosedCube_SHT31D.h"

ClosedCube_SHT31D sht3xd;
SHT31D sht_result;

void key_scan(void);
void wifi_stat_led(int *cnt);
void myserialEvent();

#define PIN_BTN PA0
#define PIN_LED PB2

int time_cnt = 0, cnt = 0, init_flag = 0;

int temp_indoor;
int humidity_indoor;

int temp_outdoor;
int humidity_outdoor;
void dispColor();

void setup()
{
    pinMode(PIN_SHT_RST, OUTPUT);
    digitalWrite(PIN_SHT_RST, HIGH);
    pinMode(PIN_SHT_ALERT, INPUT);

    pinMode(PIN_WB3_RST, OUTPUT);
    pinMode(PIN_WB3_EN, OUTPUT);

    digitalWrite(PIN_WB3_RST, 0);
    digitalWrite(PIN_WB3_EN, 0);
    digitalWrite(PIN_WB3_EN, 1);
    delay(100);

    digitalWrite(PIN_WB3_RST, 1);

    // tft.init();
    tft.initR(INITR_MINI160x80); // Init ST7735S mini display
    tft.fillScreen(ST77XX_WHITE);

    // large block of text
    //   tft.fillScreen(ST77XX_WHITE);
    //   testdrawtext("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Curabitur adipiscing ante sed nibh tincidunt feugiat. Maecenas enim massa, fringilla sed malesuada et, malesuada sit amet turpis. Sed porttitor neque ut ante pretium vitae malesuada nunc bibendum. Nullam aliquet ultrices massa eu hendrerit. Ut sed nisi lorem. In vestibulum purus a tortor imperdiet posuere. ", ST77XX_WHITE);
    //   delay(1000);
    tft.fillScreen(ST77XX_WHITE);
    tft.setTextColor(ST77XX_BLACK);
    tft.setCursor(0, 0);
    pinMode(PIN_BTN, INPUT_PULLDOWN); // 重置Wi-Fi按键初始化
    pinMode(PIN_LED, OUTPUT);         // Wi-Fi状态指示灯初始化
    wifi_protocol_init();

    Serial.begin(115200);

    pixels.begin();

    Wire.begin();
    sht3xd.begin(0x44); // I2C address: 0x44 or 0x45
    tft.setCursor(0, 0);

    delay(1000);

    pixels.clear(); // Set all pixel colors to 'off'

    // The first NeoPixel in a strand is #0, second is 1, all the way up
    // to the count of pixels minus one.
    for (int i = 0; i < (NUM_INDOOR + NUM_OUTDOOR); i++)
    { // For each pixel...

        // pixels.Color() takes RGB values, from 0,0,0 up to 255,255,255
        // Here we're using a moderately bright green color:
        pixels.setPixelColor(i, pixels.Color(0, 55, 0));

        pixels.show(); // Send the updated pixel colors to the hardware.

        delay(50); // Pause before next pass through loop
    }

    delay(1000);
    pixels.clear(); // Set all pixel colors to 'off'
    delay(100);
    pixels.clear(); // Set all pixel colors to 'off'
}
uint32_t t_old, t_current;
void loop()
{
    t_current = millis();
    if ((t_current - t_old) >= 1000 * 2)
    {
        t_old = t_current;

        sht_result = sht3xd.readTempAndHumidity(SHT3XD_REPEATABILITY_LOW, SHT3XD_MODE_CLOCK_STRETCH, 50);
        if (sht_result.error == SHT3XD_NO_ERROR)
        {

            temp_indoor = sht_result.t * 10 / 1;

            humidity_indoor = sht_result.rh / 1;

            if (tft.getCursorY() >= 150)
            {

                tft.fillScreen(ST77XX_WHITE);
                tft.setCursor(0, 0);
            }
            tft.setCursor(0, 0);
            tft.printf("\ntemp: %d", temp_indoor);
            tft.printf("\nhumidity: %d", humidity_indoor);
        }
        else
        {
            sht3xd.reset();
        }
    }

    if (init_flag == 0)
    {
        time_cnt++;
        if (time_cnt % 6000 == 0)
        {
            time_cnt = 0;
            cnt++;
        }
        wifi_stat_led(&cnt); // Wi-Fi状态处理
    }
    else
    {
        time_cnt++;
        if (time_cnt % 6000 == 0)
        {
            time_cnt = 0;
            cnt++;
            all_data_update();
        }
    }
    wifi_uart_service();
    myserialEvent(); // 串口接收处理
    key_scan();      // 重置配网按键检测

    dispColor();
}
void dispColor()
{
    uint32_t delay_ms_indoor = 60;
    uint32_t delay_ms_outdoor = 60;
    // if(temp_indoor)
    uint16_t hue_indoor = map(temp_indoor / 10, -10, 40, 43690, 0);
    uint16_t hue_outdoor = map(temp_outdoor, -10, 40, 43690, 0);

    static uint32_t val_indoor = 0;
    static uint32_t val_outdoor = 0;

    static uint32_t old_indoor;
    static uint32_t old_outdoor;

    static uint32_t speed_indoor;
    static uint32_t speed_outdoor;
    static uint8_t direction_indoor = 0; //0：上升  1：下降
    static uint8_t direction_outdoor = 0;

    uint32_t current = millis();

    speed_indoor = map(humidity_indoor, 0, 100, 2, 20);
    speed_outdoor = map(humidity_outdoor, 0, 100, 2, 20);

    if (current - old_indoor >= delay_ms_indoor)
    {
        old_indoor = current;

        val_indoor += speed_indoor;
        uint32_t color_indoor = pixels.ColorHSV(hue_indoor, 255, pixels.sine8(val_indoor));

        //为不同区域显示颜色
        for (uint8_t i = 1; i < NUM_INDOOR; i++)
        {
            pixels.setPixelColor(i, color_indoor);
        }
        // pixels.show();
        // tft.setCursor(0, 100);
        // tft.printf("in: %d,%d", hue_indoor, val_indoor);

        val_outdoor += speed_outdoor;

        uint32_t color_outdoor = pixels.ColorHSV(hue_outdoor, 255, pixels.sine8(val_outdoor));
        for (uint8_t i = NUM_INDOOR; i < (NUM_INDOOR + NUM_OUTDOOR); i++)
        {
            pixels.setPixelColor(i, color_outdoor);
        }
        pixels.show();
    }

    if (current - old_outdoor >= delay_ms_outdoor)
    {
        old_outdoor = current;

        // tft.setCursor(0, 120);
        // tft.printf("out: %d,%d", hue_outdoor, val_outdoor);
    }
}
bool flag = false;
uint8_t frame_check[256];
uint8_t frame_index = 0;
uint16_t length;
void myserialEvent()
{
    if (Serial.available())
    {
        unsigned char ch = (unsigned char)Serial.read();
        uart_receive_input(ch);

        //parse weather 0x21
        frame_check[frame_index] = ch;
        if (ch == 0x55 && frame_index == 0)
        {
            frame_index++;
        }
        else if (ch == 0xaa && frame_index == 1)
        {
            frame_index++;
        }
        else if (ch == 0x00 && frame_index == 2)
        {
            frame_index++;
        }
        else if (ch == 0x21 && frame_index == 3)
        {
            frame_index++;
        }
        else
        {
            if (frame_index >= 4)
            {
                frame_index++;
                if (frame_index == 6)
                {

                    flag = true;
                    length = ((frame_check[4] << 8) & 0xff00) | frame_check[5];
                    if (tft.getCursorY() >= 150)
                    {

                        tft.fillScreen(ST77XX_WHITE);
                        tft.setCursor(0, 0);
                    }
                    tft.setCursor(0, 110);
                    tft.printf("length: %d", length);
                }
                if (frame_index == 22)
                {
                    temp_outdoor = (frame_check[18] << 24) + (frame_check[19] << 16) + (frame_check[20] << 8) + (frame_check[21] << 0);
                    if (tft.getCursorY() >= 150)
                    {

                        tft.fillScreen(ST77XX_WHITE);
                        tft.setCursor(0, 0);
                    }
                    tft.setCursor(0, 125);
                    tft.printf("temp: %d", temp_outdoor);
                }
                if (frame_index == 41)
                {

                    humidity_outdoor = (frame_check[37] << 24) + (frame_check[38] << 16) + (frame_check[39] << 8) + (frame_check[40] << 0);
                    if (tft.getCursorY() >= 150)
                    {

                        tft.fillScreen(ST77XX_WHITE);
                        tft.setCursor(0, 0);
                    }
                    tft.setCursor(0, 140);
                    tft.printf("humidity: %d%% ", humidity_outdoor);
                    frame_index = 0;
                    memset(frame_check, 0, sizeof(frame_check));
                }
            }
        }
    }
}
void key_scan(void)
{
    static char ap_ez_change = 0;
    unsigned char buttonState = LOW;
    buttonState = digitalRead(PIN_BTN);
    if (buttonState == HIGH)
    {
        delay(3000);
        buttonState = digitalRead(PIN_BTN);
        // printf("----%d", buttonState);
        if (buttonState == HIGH)
        {
            // printf("123\r\n");
            //需要给出指示
            init_flag = 0;
            switch (ap_ez_change)
            {
            case 0:
                mcu_set_wifi_mode(SMART_CONFIG);
                break;
            case 1:
                mcu_set_wifi_mode(AP_CONFIG);
                break;
            default:
                break;
            }
            ap_ez_change = !ap_ez_change;
        }
    }
}

void wifi_stat_led(int *cnt)
{

    switch (mcu_get_wifi_work_state())
    {
    case SMART_CONFIG_STATE: //0x00
        init_flag = 0;
        if (*cnt == 2)
        {
            *cnt = 0;
        }
        if (*cnt % 2 == 0) //LED快闪
        {
            digitalWrite(PIN_LED, 1);
        }
        else
        {
            digitalWrite(PIN_LED, 0);
        }
        break;
    case AP_STATE: //0x01
        init_flag = 0;
        if (*cnt >= 30)
        {
            *cnt = 0;
        }
        if (*cnt == 0) // LED 慢闪
        {
            digitalWrite(PIN_LED, 1);
        }
        else if (*cnt == 15)
        {
            digitalWrite(PIN_LED, 0);
        }
        break;

    case WIFI_NOT_CONNECTED:      // 0x02
        digitalWrite(PIN_LED, 0); // LED 熄灭
        break;
    case WIFI_CONNECTED: // 0x03
        break;
    case WIFI_CONN_CLOUD: // 0x04
        if (0 == init_flag)
        {
            digitalWrite(PIN_LED, 1); // LED 常亮
            init_flag = 1;            // Wi-Fi 连接上后该灯可控
            *cnt = 0;
        }

        break;

    default:
        digitalWrite(PIN_LED, 0);
        break;
    }
}