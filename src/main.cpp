// 硬件版本： ESP32便携点焊机-2.1
// 软件版本： 2.1
// 创建时间： 熵 2023-05-02
// 编译环境： arduino1.8.19   esp32_package_2.0.5
// 编译前需要安装相关库 和选择对应的开发板芯片设置
/*
Arduino IDE ESP32 C3  芯片烧录配置：
Flash Mode: "DIO"          下载烧录
USB CDC On Boot: “Enable”  USB串口打印

ADC衰减量程参数：
ADC_ATTEN_DB_0    0~750mV
ADC_ATTEN_DB_2_5  0~1050mV
ADC_ATTEN_DB_6    0~1300mV
ADC_ATTEN_DB_11   0~2500mV
*/

#include <Arduino.h>
#include <EEPROM.h>
#include "TFT1.h"
#include "driver/adc.h"
#include "esp_adc_cal.h"
#include "driver/temp_sensor.h" //读取芯片内部温度

#define ADC_ATTEN ADC_ATTEN_DB_11
static esp_adc_cal_characteristics_t adc1_chars;

byte K1 = 8;  // 按键1
byte K2 = 9;  // 按键2
byte K3 = 10; // 按键3

byte ad0 = 0; // 电源电压采集
byte ad1 = 1; // 电池电压采集
byte ad2 = 2; // 电容电压采集
byte ad3 = 3; // 焊件分压检测

byte MOS = 7;  // MOS开关控制
byte EN0 = 0;  // 主充电容开关
byte EN1 = 20; // 辅充电容开关
byte LED = 21; // LED照明开关

int TL1 = 800; // 延时点焊时间
int TH1 = 10;  // 预热点焊时间
int TL2 = 10;  // 点焊间隔时间
int TH2 = 30;  // 脉冲点焊时间
int THX = 5;   // 脉冲连点次数
int lcd = 1;   // 屏显方向参数
int led = 0;   // 辅助照明开关
int VEN = 0;   // 电容充电模式
int VYZ = 6;   // 焊件检测阈值

float VHJ; // 焊件电压
float VCC; // 电容电压
float VBA; // 电池电压
float VBU; // 电源电压
float CES; // 芯片温度

int SZ1 = 0; // 设置模式
int MS1 = 0; // 页面模式
int CS1 = 0; // 参数
int CS2 = 0; // 参数
int CS3 = 0; // 参数
int CS4 = 0; // 参数
int CS5 = 0; // 参数

unsigned int TM1 = 0; // 时间1
unsigned int TM2 = 0; // 时间2

// ADC1和温度设置
void adc_init()
{
  esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN, ADC_WIDTH_BIT_12, 0, &adc1_chars); // 获取ADC特征值

  adc1_config_channel_atten(ADC1_CHANNEL_0, ADC_ATTEN); // 设置ADC衰减
  adc1_config_channel_atten(ADC1_CHANNEL_1, ADC_ATTEN); // 设置ADC衰减
  adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN); // 设置ADC衰减

  // 初始化温度传感器
  temp_sensor_config_t temp_sensor = {
      .dac_offset = TSENS_DAC_L2,
      .clk_div = 6,
  };
  temp_sensor_set_config(temp_sensor);
  temp_sensor_start();
}

// 写入Flash数据
void sjx(int a1, int a2, int a3, int a4, int a5, int a6, int a7, int a8, int a9)
{
  EEPROM.put(0, a1);
  EEPROM.put(4, a2);
  EEPROM.put(8, a3);
  EEPROM.put(12, a4);
  EEPROM.put(16, a5);
  EEPROM.put(20, a6);
  EEPROM.put(24, a7);
  EEPROM.put(28, a8);
  EEPROM.put(32, a9);
  EEPROM.commit(); // 保存更改的数据
  delay(1);
}

// 读取Flash数据
void sjd(int &a1, int &a2, int &a3, int &a4, int &a5, int &a6, int &a7, int &a8, int &a9)
{
  EEPROM.begin(1024);
  delay(1);
  EEPROM.get(0, a1);
  EEPROM.get(4, a2);
  EEPROM.get(8, a3);
  EEPROM.get(12, a4);
  EEPROM.get(16, a5);
  EEPROM.get(20, a6);
  EEPROM.get(24, a7);
  EEPROM.get(28, a8);
  EEPROM.get(32, a9);

  // isnan(x) 如果浮点数x是nan返回1，如果不是返回0
  if (isnan(a1) == 1 || isnan(a2) == 1 || a1 < 0)
  {
    sjx(800, 10, 10, 30, 5, 1, 0, 0, 6);
    a1 = 800;
    a2 = 10;
    a3 = 10;
    a4 = 30;
    a5 = 5;
    a6 = 1;
    a7 = 0;
    a8 = 0;
    a9 = 6;
  }
  if (a2 > 200 || a3 > 100 || a7 > 1 || a8 > 5 || a9 > 50 || a9 < 0)
  {
    a1 = 800;
    a2 = 10;
    a3 = 10;
    a4 = 30;
    a5 = 5;
    a6 = 1;
    a7 = 0;
    a8 = 0;
    a9 = 6;
    sjx(800, 10, 10, 30, 5, 1, 0, 0, 6);
  }
}

// 按键函数
void key1(int &a, int ki, int i)
{
  if (digitalRead(ki) == LOW)
  {
    a++;
    if (a > i)
    {
      a = 0;
    }
    while (digitalRead(ki) == LOW)
    {
    }
  }
}

// 按键函数(传递引用, 最小值, 最大值, 加减按键, 步进值)
void key3(int &a, int mi, int mx, int kh, int xi)
{
  if (digitalRead(kh) == LOW)
  {
    long int t = millis();
    int ai = a;
    if (digitalRead(kh) == LOW)
    {
      a++;
      if (a > mx)
      {
        a = mi;
      }
      while (digitalRead(kh) == LOW)
      {
        if (millis() - t > 500)
        {
          a = ai + xi;
          if (a > mx)
          {
            a = mx;
          }
          break;
        }
      }
    }
    sjx(TL1, TH1, TL2, TH2, THX, lcd, led, VEN, VYZ);
  }
}

// 电压测量
void VADC()
{
  pinMode(ad0, INPUT);
  int en0 = digitalRead(ad0);
  float v0 = analogRead(ad0);
  float v1 = analogRead(ad1);
  float v2 = analogRead(ad2);
  float v3 = analogRead(ad3);
  pinMode(EN0, OUTPUT);
  digitalWrite(EN0, en0);

  v0 = esp_adc_cal_raw_to_voltage(v0, &adc1_chars);
  v1 = esp_adc_cal_raw_to_voltage(v1, &adc1_chars);
  v2 = esp_adc_cal_raw_to_voltage(v2, &adc1_chars);
  v3 = esp_adc_cal_raw_to_voltage(v3, &adc1_chars);

  VBU = v0 * 13 / 1000;
  VBA = v1 * 2 / 1000;
  VCC = v2 / 1000;
  VHJ = v3 * 2 / 1000;

  temp_sensor_read_celsius(&CES); // 芯片内部温度测量
  // Serial.print("VHJ:");  Serial.println(VHJ);
  // Serial.print("VCC:");  Serial.println(VCC);
  // Serial.print("VBA:");  Serial.println(VBA);
  // Serial.print("V5V:");  Serial.println(V5V);
}

// 自动模式
void VGS()
{
  float vyz = float(VYZ) / 10;
  if (VHJ > vyz || CS1 > 0)
  {
    CS1 = 1;
    int th1 = TL1 + TH1;
    int TLH = (TL2 + TH2) * THX + th1;
    int tmx = millis() - TM2;

    if (tmx <= TLH)
    {
      digitalWrite(EN1, LOW);
      if (tmx < TL1)
      {
        digitalWrite(MOS, LOW);
      }
      else if (tmx < th1)
      {
        digitalWrite(MOS, HIGH);
      }
      else if (CS2 < THX)
      {
        int tlx = (TL2 + TH2) * CS2 + TL2 + th1;
        int thx = (TL2 + TH2) * CS2 + TL2 + TH2 + th1;
        if (tmx < tlx)
        {
          digitalWrite(MOS, LOW);
        }
        else if (tmx < thx)
        {
          digitalWrite(MOS, HIGH);
        }
        else
        {
          CS2++;
        }
      }
      else if (CS2 <= THX)
      {
        CS2 = THX + 1;
        CS3++;
        if (CS3 > 99)
        {
          CS3 = 99;
        }
      }
    }
    else
    {
      CS1 = 0;
      CS2 = 0;
    }
  }
  else
  {
    TM2 = millis();
    digitalWrite(MOS, LOW);
  }
}

// 手动模式
void MODE()
{
  key1(CS4, K2, 1);
  if (CS4 == 0)
  {
    if (digitalRead(K3) == LOW)
    {
      CS3 = 1;
    }
    if (CS3 > 0)
    {
      float vyz = float(VYZ) / 10;
      if (VHJ > vyz || CS1 > 0)
      {
        CS1 = 1;
        int th1 = TL1 + TH1;
        int TLH = (TL2 + TH2) * THX + th1;
        int tmx = millis() - TM2;
        if (tmx <= TLH)
        {
          digitalWrite(EN1, LOW);
          if (tmx < TL1)
          {
            digitalWrite(MOS, LOW);
          }
          else if (tmx < th1)
          {
            digitalWrite(MOS, HIGH);
          }
          else if (CS2 < THX)
          {
            int tlx = (TL2 + TH2) * CS2 + TL2 + th1;
            int thx = (TL2 + TH2) * CS2 + TL2 + TH2 + th1;
            if (tmx < tlx)
            {
              digitalWrite(MOS, LOW);
            }
            else if (tmx < thx)
            {
              digitalWrite(MOS, HIGH);
            }
            else
            {
              CS2++;
            }
          }
        }
        else
        {
          CS1 = 0;
          CS2 = 0;
          CS3 = 0;
        }
      }
    }
    else
    {
      TM2 = millis();
      digitalWrite(MOS, LOW);
    }
  }
  else if (CS4 == 1)
  {
    if (digitalRead(K3) == LOW)
    {
      digitalWrite(EN1, LOW);
      digitalWrite(MOS, HIGH);
      CS1 = 1;
    }
    else
    {
      digitalWrite(MOS, LOW);
      CS1 = 0;
    }
  }
}

// 点焊参数设置
void DHSZ()
{
  key1(CS1, K2, 7);
  if (CS1 == 0)
  {
    key3(TL1, 0, 2000, K3, 200);
  }
  else if (CS1 == 1)
  {
    key3(TH1, 0, 200, K3, 20);
  }
  else if (CS1 == 2)
  {
    key3(TH2, 0, 200, K3, 20);
  }
  else if (CS1 == 3)
  {
    key3(TL2, 0, 100, K3, 10);
  }
  else if (CS1 == 4)
  {
    key3(THX, 1, 50, K3, 10);
  }
  else if (CS1 == 5)
  {
    key3(lcd, 0, 1, K3, 1);
  }
  else if (CS1 == 6)
  {
    key3(led, 0, 1, K3, 1);
    if (led == 0)
    {
      digitalWrite(LED, HIGH);
    }
    else
    {
      digitalWrite(LED, LOW);
    }
  }
  else if (CS1 == 7)
  {
    key3(VYZ, 0, 50, K3, 10);
  }
}

// 电容充电开关控制和过温保护
void CVEN()
{
  if (CES > 50)
  {
    CS5 = 1;
  }
  else if (CES < 48)
  {
    CS5 = 0;
  }
  if (CS5 == 0 && VCC < 2.72)
  {
    if (VEN == 0)
    {
      if (VBA > 3.5 && VCC < 0.6)
      {
        digitalWrite(EN0, LOW);
        digitalWrite(EN1, HIGH);
      }
      else if (VBU > 4.3 && VCC >= 0.6)
      {
        digitalWrite(EN0, HIGH);
        digitalWrite(EN1, LOW);
      }
      else if (VBA > 3.5 && VBU < 4.3)
      {
        digitalWrite(EN0, LOW);
        digitalWrite(EN1, HIGH);
      }
      else
      {
        digitalWrite(EN0, LOW);
        digitalWrite(EN1, LOW);
      }
    }
    else if (VEN == 1)
    {
      digitalWrite(EN0, HIGH);
      digitalWrite(EN1, LOW);
    }
    else if (VEN == 2)
    {
      digitalWrite(EN0, LOW);
      digitalWrite(EN1, HIGH);
    }
    else if (VEN == 3)
    {
      digitalWrite(EN0, HIGH);
      digitalWrite(EN1, HIGH);
    }
    else
    {
      digitalWrite(EN0, LOW);
      digitalWrite(EN1, LOW);
    }
  }
  else
  {
    digitalWrite(EN0, LOW);
    digitalWrite(EN1, LOW);
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(K1, INPUT_PULLUP);
  pinMode(K2, INPUT_PULLUP);
  pinMode(K3, INPUT_PULLUP);

  pinMode(ad1, INPUT);
  pinMode(ad2, INPUT);
  pinMode(ad3, INPUT);

  pinMode(LED, OUTPUT);
  pinMode(EN0, OUTPUT);
  pinMode(EN1, OUTPUT);
  pinMode(MOS, OUTPUT);

  digitalWrite(MOS, LOW);
  digitalWrite(EN0, LOW);
  digitalWrite(EN1, LOW);
  digitalWrite(LED, HIGH);

  delay(100);

  sjd(TL1, TH1, TL2, TH2, THX, lcd, led, VEN, VYZ); // 读取配置信息
  adc_init();                                       // ADC校准设置
  csh(lcd);                                         // 屏幕初始化并传入显示方向
  if (led == 0)
  {
    digitalWrite(LED, HIGH);
  }
  else
  {
    digitalWrite(LED, LOW);
  }
  TM1 = millis();
}

void loop()
{
  key1(MS1, K1, 1);
  if (MS1 == 0)
  {
    key1(SZ1, K2, 3);
  }
  else if (SZ1 == 0)
  {
    VGS();
  }
  else if (SZ1 == 1)
  {
    MODE();
  }
  else if (SZ1 == 2)
  {
    DHSZ();
  }
  else if (SZ1 == 3)
  {
    key3(VEN, 0, 4, K3, 2);
  }

  if ((millis() - TM1) > 80)
  {
    VADC();
    CVEN();
    if (MS1 == 0)
    {
      ym1(VBU, VBA, VCC, CES, float(SZ1));
      CS1 = 0;
      CS2 = 0;
      CS3 = 0;
    }
    else if (SZ1 == 0)
    {
      ms0(VBA, VCC, VHJ, CES, float(VYZ), float(CS1), float(CS3));
    }
    else if (SZ1 == 1)
    {
      ms1(VBA, VCC, VHJ, CES, float(VYZ), float(CS1), float(CS4));
    }
    else if (SZ1 == 2)
    {
      sz2(TL1, TH1, TL2, TH2, THX, lcd, led, VYZ, CS1);
    }
    else if (SZ1 == 3)
    {
      sz3(VBU, VBA, VCC, float(VEN));
    }
    TM1 = millis();
  }
}

//
