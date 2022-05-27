#include <Arduino.h>
#include <U8g2lib.h>

#include "RTClib.h"

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

RTC_DS3231 rtc;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

/*
 * 提醒喝水小助手的源代码
 * 核心功能：如果红外检测处于高电平就会计时，计时到一定程度就会自动提醒。
 * 拓展功能：
 * - 利用 DS3231 时间模块能够检测时间和温度，并且利用该
 * 变量：统计剩余时间数，已经喝水次数，以往喝水的加权平均，上次喝水的时间
 * 一些特性：只有在放回水杯之后才会刷新统计量，所以拿起水杯的时候不能马上看到屏幕更新
 */

const int wuwuwu = 13; //提醒接口
const int censor = 4;  //红外检测器
// char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};

const int DEADLINE = 60;      //设定的分钟数，建议设置 60 分钟
const float lambda = 0.6;     //加权权重，权重越大越在意最近的喝水间隔
float averge_time = DEADLINE; //以往喝水间隔加权平均
int last_time = DEADLINE;     //上次喝水的时间

int count_down = DEADLINE; //倒计时，单位是分
int fresh_count = 0;       // 刷新计时
float percent = 1.0;       //剩余倒计时百分比

int length = 80; //矩阵长度

void setup(void)
{
  u8g2.begin();
  pinMode(wuwuwu, OUTPUT);
  pinMode(censor, INPUT);
  Serial.begin(9600);

  // 初始化rtc，
  if (!rtc.begin())
  {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort(); // 如果没有 RTC 停止运行
  }

  if (rtc.lostPower())
  {
    Serial.println("RTC lost power, let's set the time!");
    // When time needs to be set on a new device, or after a power loss, the
    // following line sets the RTC to the date & time this sketch was compiled
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    // This line sets the RTC with an explicit date & time, for example to set
    // January 21, 2014 at 3am you would call:
    // rtc.adjust(DateTime(2014, 1, 21, 3, 0, 0));
  }

  DateTime now = rtc.now();// 一开始要显示初始化，不然一开始会黑屏
  draw(now);
}

void loop(void)
{
  if (digitalRead(censor) == LOW)
  {
    while (digitalRead(censor) != HIGH) // 如果拿起水杯，则此循环等待水杯放回，放回后才会刷新统计量
    {
      delay(1000);
    }

    freshArgs(); //更新统计量
  }

  if (fresh_count == 0) //每分钟更新一次数据，同时刷新led屏幕
  {
    fresh_count = 60; //一分钟更新一次

    if (count_down > 0)
    {
      last_time = DEADLINE - count_down; //上次喝水的时间，单位为分钟
      percent = static_cast<float>(count_down) / DEADLINE;
    }
    else if (!count_down)
    {
      ring();
    }

    DateTime now = rtc.now();
    draw(now);

    count_down--;
  }

  fresh_count--;

  delay(100); //使用 1000 的参数来设置 1 秒的循环
}

void draw(DateTime now) //更新 oled 屏幕显示
{
  u8g2.clearBuffer(); //清除缓存
  u8g2.setFont(u8g2_font_7x14B_tf);

  u8g2.setCursor(0, 13);
  u8g2.print("Year: ");
  u8g2.print(now.year());
  // u8g2.print("!!NotImplemented!!");

  u8g2.setCursor(0, 13 + 13);
  u8g2.print("Average: ");
  u8g2.print(int(averge_time));
  u8g2.print(" mins");

  u8g2.setCursor(0, 26 + 13);
  u8g2.print("Last: ");
  u8g2.print(last_time);
  u8g2.print(" mins ago");

  u8g2.setCursor(0, 50);
  u8g2.print("water force:");
  u8g2.drawRFrame(0, 52, length, 10, 2);
  u8g2.drawBox(0, 52, length * percent, 10);

  u8g2.setCursor(length + 2, 62);
  u8g2.print(int(percent * 100));
  u8g2.print("%");

  u8g2.sendBuffer(); //显示
}

void freshArgs()
{
  count_down = DEADLINE;
  fresh_count = 0;
  averge_time = averge_time * (1 - lambda) + last_time * lambda;
  last_time = 0;
}

void ring()
{
  while (digitalRead(censor) == HIGH) // 鸣叫
  {
    digitalWrite(wuwuwu, HIGH);
    delay(300);
    digitalWrite(wuwuwu, LOW);
    delay(300);
  }
}
