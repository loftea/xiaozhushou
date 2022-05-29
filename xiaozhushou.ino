#include <Arduino.h>
#include <U8g2lib.h>

#include "RTClib.h"

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

RTC_DS3231 rtc;
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

/*
   提醒喝水小助手的源代码
   核心功能：如果红外检测处于高电平就会计时，计时到一定时间就会自动提醒。
   拓展功能：
   - 利用 DS3231 时间模块能够检测时间和温度，并且利用以上信息进行健康提醒
   变量：统计剩余时间数，已经喝水次数，以往喝水的加权平均，上次喝水的时间
   一些特性：
   - 只有在放回水杯之后才会刷新统计量，所以拿起水杯的时候不能马上看到屏幕更新
   - 使用的平均方式是指数加权平均，节约空间，并且更加科学
   - 在睡觉时间会自动关闭鸣叫功能
*/

const int wuwuwu = 13;   //提醒接口
const int censor = 4;    //红外检测器
const int DEADLINE = 60; //设定的分钟数，建议设置 60 分钟
const float lambda = 0.6; //加权权重，权重越大越在意最近的喝水间隔

// char daysOfTheWeek[7][12] = {"Sunday", "Monday", "Tuesday", "Wednesday",
// "Thursday", "Friday", "Saturday"};

float averge_time = DEADLINE; //以往喝水间隔加权平均
int last_time = DEADLINE;     //上次喝水的时间

int count_down = DEADLINE; //倒计时，单位是分
int fresh_count = 0;       // 刷新计时
float percent = 1.0;       //剩余倒计时百分比

int length = 80; //矩阵长度

bool is_on = true; //表示程序正在运行，防止鸣叫时间过长的信息量

void setup(void) {
  u8g2.begin();
  pinMode(wuwuwu, OUTPUT);
  pinMode(censor, INPUT);
  Serial.begin(9600);

  // 初始化rtc，
  if (!rtc.begin()) {
    //    Serial.println("Couldn't find RTC");
    //    Serial.flush();
    abort(); // 如果没有 RTC 停止运行
  }

  if (rtc.lostPower()) {
    //    Serial.println("RTC lost power, let's set the time!");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // 一开始要显示初始化，不然一开始一分钟会黑屏
  draw();
}

void loop(void) {
  if (!is_on) {
    drawError();
    while (digitalRead(censor) ==
           HIGH) // 如果红外线被意外遮挡，会一直等待红外移开
    {
      delay(1000);
    }
    is_on = true;
  }

  if (digitalRead(censor) == LOW) {
    while (digitalRead(censor) !=
           HIGH) // 如果拿起水杯，则此循环等待水杯放回，放回后才会刷新统计量
    {
      delay(1000);
    }

    freshArgs(); //更新统计量
  }

  if (fresh_count == 0) //每分钟更新一次数据，同时刷新led屏幕
  {
    fresh_count = 60; //一分钟更新一次

    if (count_down > 0) {
      last_time = DEADLINE - count_down; //上次喝水的时间，单位为分钟
      percent = static_cast<float>(count_down) / DEADLINE;
    } else if (!count_down) {
      ring();
    }

    draw();

    count_down--;
  }

  fresh_count--;

  delay(20); //使用 1000 的参数来设置 1 秒的循环
}

void draw() //更新 oled 屏幕显示
{
  DateTime now = rtc.now();

  u8g2.clearBuffer(); //清除缓存
  u8g2.setFont(u8g2_font_7x14B_tf);

  u8g2.setCursor(0, 13);
  u8g2.print(now.year());
  u8g2.print("/");
  u8g2.print(now.month());
  u8g2.print("/");
  u8g2.print(now.day());

  u8g2.print(" ");
  u8g2.print(int(rtc.getTemperature()));
  u8g2.print(" C"); // 显示温度

  // 下面的功能显示星期，需要在全局变量中启用星期的数组
  // u8g2.print(" ");
  // u8g2.print(daysOfTheWeek[now.week()]);

  // u8g2.print("!!NotImplemented!!");

  u8g2.setCursor(0, 13 + 13);
  u8g2.print("Average: ");
  u8g2.print(int(averge_time));
  u8g2.print(" mins");

  u8g2.setCursor(0, 26 + 13);
  u8g2.print("Last: ");
  u8g2.print(last_time);
  u8g2.print(" mins ago");

  u8g2.setCursor(0, 52);
  uint8_t h = now.hour();
  //  u8g2.print(h);
  if (h == 8) {
    u8g2.print("  GOOD MORNING!");
  } else if (h < 9) {
    u8g2.print("    zzZ");
  }    else if (h >= 22) {
    u8g2.print("   GOOD NIGHT!");
  } else if (averge_time > 50) {
    u8g2.print("   Be Active!");
  } else {
    u8g2.print("  Water Matters!");
  }

  u8g2.drawRFrame(0, 54, length, 10, 2); // 绘制矩阵
  u8g2.drawBox(0, 54, length * percent, 10);

  u8g2.setCursor(length + 2, 64);
  u8g2.print(int(percent * 100));
  u8g2.print("%");

  u8g2.sendBuffer(); //显示
}

void freshArgs() {
  count_down = DEADLINE;
  fresh_count = 0;
  averge_time = averge_time * (1 - lambda) + last_time * lambda;
  last_time = 0;
}

void ring() {
  int tmp = 0;                                    //防止鸣叫时间过长
  while (digitalRead(censor) == HIGH && tmp < 30) // 鸣叫
  {
    if (rtc.now().hour() < 10 || rtc.now().hour() >= 23 ) {
      continue;// 如果在睡觉时间，不会鸣叫
    }
    digitalWrite(wuwuwu, HIGH);
    delay(300);
    digitalWrite(wuwuwu, LOW);
    delay(300);

    tmp++;
  }

  if (tmp >= 30) {
    is_on = false;
  }
}

void drawError() { //提示用户需要移开遮挡物
  u8g2.clearBuffer(); //清除缓存
  u8g2.setFont(u8g2_font_7x14B_tf);

  u8g2.setCursor(0, 34);
  u8g2.print("  LASER COVERED!");

  u8g2.sendBuffer();
}
