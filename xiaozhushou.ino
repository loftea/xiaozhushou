#include <Arduino.h>
#include <U8g2lib.h>

#ifdef U8X8_HAVE_HW_I2C
#include <Wire.h>
#endif

/*
 * 提醒喝水小助手的源代码
 * 核心功能：如果红外检测处于高电平就会计时，计时到一定程度就会自动提醒。
 * 变量：统计剩余时间数，已经喝水次数，以往喝水的加权平均，上次喝水的时间
 *
 */

const int wuwuwu = 13;                          //提醒接口
const int censor = 4;                           //红外检测器
const int DEADLINE = 60;                        //设定的分钟数，建议设置 60 分钟
const float lambda = static_cast<float>(2) / 3; //加权权重
float averge_time = DEADLINE;
//以往喝水时间加权平均，计算公式为 $avr <- time_new * lambda +avr_old * (1-\lambda)$
int last_time = DEADLINE;  //上次喝水的时间
int count_down = DEADLINE; //倒计时，单位是分
int length = 80;           //矩阵长度
float percent = 1.0;
int fresh_count = 0;

auto prevRead = HIGH;

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

void setup(void)
{
  u8g2.begin();
  pinMode(wuwuwu, OUTPUT);
  pinMode(censor, INPUT);
  //  digitalWrite(wuwuwu,HIGH);
}

void loop(void)
{
  if (digitalRead(censor) == LOW)
  {
    count_down = DEADLINE * 60;
    fresh_count = 60;
    digitalWrite(wuwuwu, LOW);
    if (prevRead == HIGH)
    {
      averge_time = averge_time * (1 - lambda) + last_time * lambda;
    }
    prevRead = LOW;
  }
  else
  {
    prevRead = HIGH;
  }

  if (fresh_count == 0)
  {
    fresh_count = 60; //一分钟更新一次
    if (count_down == 0)
    {
      while (digitalRead(censor) == HIGH) // 鸣叫
      {
        digitalWrite(wuwuwu, HIGH);
        delay(300);
        digitalWrite(wuwuwu, LOW);
        delay(300);
      }
      count_down = DEADLINE + 1;
    }

    if (count_down > 0)
    {
      last_time = DEADLINE - count_down; //上次喝水的时间，单位为分钟
      percent = static_cast<float>(count_down) / DEADLINE;
      count_down--;
    }

    u8g2.clearBuffer(); //清除缓存
    u8g2.setFont(u8g2_font_7x14B_tf);

    u8g2.setCursor(0, 13);
    u8g2.print("Average: ");
    u8g2.print(int(averge_time));
    u8g2.print(" mins");
    
    u8g2.setCursor(0, 26);
    u8g2.print("Last: ");
    u8g2.print(last_time);
    u8g2.print(" mins ago");

    u8g2.setCursor(0, 50);
    u8g2.print("water force:");
    u8g2.drawRFrame(0, 52, length, 10, 2);
    u8g2.drawBox(0, 52, length * percent, 10);
    
    u8g2.setCursor(length + 2, 62);
    u8g2.print(int(percent*100));
    u8g2.print("%");

    u8g2.sendBuffer(); //显示
  }
  fresh_count--;

  delay(100);
}
