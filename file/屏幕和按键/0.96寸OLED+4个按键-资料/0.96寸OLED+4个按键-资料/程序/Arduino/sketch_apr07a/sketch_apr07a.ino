#include <Adafruit_GFX.h>     //包含图形库
#include <Adafruit_SSD1306.h> //包含SSD1306 OLED驱动库
#include <Wire.h>             //包含IIC通讯库

Adafruit_SSD1306 display(128, 64, &Wire, -1);                               //创建一个长128像素 宽64像素 硬件IIC通讯 没有复位引脚的屏幕对象

void setup()
{
  Serial.begin(9600);                             //初始化串口通讯 波特率9600
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) //如果没有正常初始化IIC地址0x3C的屏幕
  {
    Serial.print("SSD1306 allocation failed"); //串口打印报错信息
    for (;;)                                   //死循环
      ;
  }

  display.clearDisplay(); //清空屏幕数据缓冲区
  display.display();      //输出缓冲区数据至屏幕

  // initialize the pushbutton pin as an input:
  pinMode(9, INPUT);
  pinMode(10, INPUT);
  pinMode(11, INPUT);
  pinMode(12, INPUT);
}

void loop()
{  
  if(digitalRead(9)==0)
  {
    display.clearDisplay();              //清空屏幕数据缓冲区
    display.setTextSize(3);              //设置字体大小为1:3
    display.setTextColor(SSD1306_WHITE); //设置字体颜色为白色
    display.setCursor(0, 0);             //设置显示位置(该位置为字符的左上角坐标)
    display.print("key1");            //打印输入的字符
    display.display();                   //刷新屏幕
  }
  if(digitalRead(10)==0)
  {
    display.clearDisplay();              //清空屏幕数据缓冲区
    display.setTextSize(3);              //设置字体大小为1:3
    display.setTextColor(SSD1306_WHITE); //设置字体颜色为白色
    display.setCursor(0, 0);             //设置显示位置(该位置为字符的左上角坐标)
    display.print("key2");            //打印输入的字符
    display.display();                   //刷新屏幕
  }
  if(digitalRead(11)==0)
  {
    display.clearDisplay();              //清空屏幕数据缓冲区
    display.setTextSize(3);              //设置字体大小为1:3
    display.setTextColor(SSD1306_WHITE); //设置字体颜色为白色
    display.setCursor(0, 0);             //设置显示位置(该位置为字符的左上角坐标)
    display.print("key3");            //打印输入的字符
    display.display();                   //刷新屏幕
  }
  if(digitalRead(12)==0)
  {
    display.clearDisplay();              //清空屏幕数据缓冲区
    display.setTextSize(3);              //设置字体大小为1:3
    display.setTextColor(SSD1306_WHITE); //设置字体颜色为白色
    display.setCursor(0, 0);             //设置显示位置(该位置为字符的左上角坐标)
    display.print("key4");            //打印输入的字符
    display.display();                   //刷新屏幕
  }
  delay(10);
}
