/************************************************************************************
*  Copyright (c), 2014, HelTec Automatic Technology co.,LTD.
*            All rights reserved.
*
* Http:    www.heltec.cn
* Email:   cn.heltec@gmail.com
* WebShop: heltec.taobao.com
*
* File name: main.c
* Project  : HelTec.uvprij
* Processor: STM32F103C8T6
* Compiler : MDK fo ARM
* 
* Author : 小林
* Version: 1.00
* Date   : 2014.4.8
* Email  : hello14blog@gmail.com
* Modification: none
* 
* Description:128*64点阵的OLED显示屏测试文件，仅适用于惠特自动化(heltec.taobao.com)的SD1306驱动IIC通信方式显示屏
*
* Others: none;
*
* Function List:
*	1. int main(void);//主函数
*
* History: none;
*
*************************************************************************************/
#include "stm32f10x.h"
#include "OLED_I2C.h"
#include "delay.h"
#include "key.h"

int set_speed(int num)
{
	switch (num)
	{
		case 0: OLED_ShowStr(20+70,0,"0",2);
		        break;
		case 1: OLED_ShowStr(20+70,0,"1",2);
		        break;
		case 2: OLED_ShowStr(20+70,0,"2",2);
		        break;
		case 3: OLED_ShowStr(20+70,0,"3",2);
		        break;
		case 4: OLED_ShowStr(20+70,0,"4",2);
		        break;
		case 5: OLED_ShowStr(20+70,0,"5",2);
		        break;
		case 6: OLED_ShowStr(20+70,0,"6",2);
		        break;
		case 7: OLED_ShowStr(20+70,0,"7",2);
		        break;
		case 8: OLED_ShowStr(20+70,0,"8",2);
		        break;
		case 9: OLED_ShowStr(20+70,0,"9",2);
		        break;
		default:break;
	}
}

int set_time(int time)
{
	switch (time)
	{
		case 0: OLED_ShowStr(20+70,2,"0",2);
		        break;
		case 10: OLED_ShowStr(20+70,2,"10",2);
		        break;
		case 20: OLED_ShowStr(20+70,2,"20",2);
		        break;
		case 30: OLED_ShowStr(20+70,2,"30",2);
		        break;
		case 40: OLED_ShowStr(20+70,2,"40",2);
		        break;
		case 50: OLED_ShowStr(20+70,2,"50",2);
		        break;
		case 60: OLED_ShowStr(20+70,2,"60",2);
		        break;
		case 70: OLED_ShowStr(20+70,2,"70",2);
		        break;
		case 80: OLED_ShowStr(20+70,2,"80",2);
		        break;
		case 90: OLED_ShowStr(20+70,2,"90",2);
		        break;
		default:break;
	}
}

int main(void)
{
	unsigned char i;
	extern const unsigned char BMP1[];
	int x=0;//光标大小
	int y=0;
	unsigned char edit=0;//0=非编辑状态; 1=编辑状态
	int speed=9;
	int time=30;
	
	DelayInit();
	I2C_Configuration();
	OLED_Init();
	KEY_Init();//按键初始化
	OLED_CLS();//清屏
	
	//初始化菜单

	OLED_ShowCN(20+0,0,0);//设
	OLED_ShowCN(20+16,0,1);//置
	OLED_ShowCN(20+32,0,2);//速
	OLED_ShowCN(20+48,0,3);//度
	OLED_ShowStr(20+70,0,"9",2);
	
	OLED_ShowCN(20+0,2,0);//设
	OLED_ShowCN(20+16,2,1);//置
	OLED_ShowCN(20+32,2,4);//时
	OLED_ShowCN(20+48,2,5);//间
	OLED_ShowStr(20+70,2,"30",2);
	
	OLED_ShowCN(20+0,4,0);//设
	OLED_ShowCN(20+16,4,1);//置
	OLED_ShowCN(20+32,4,6);//密
	OLED_ShowCN(20+48,4,7);//码
	OLED_ShowStr(20+70,4,"1234",2);
	
	OLED_ShowCN(20+0,6,8);//软
	OLED_ShowCN(20+16,6,9);//件
	OLED_ShowCN(20+32,6,10);//版
	OLED_ShowCN(20+48,6,11);//本
	OLED_ShowStr(20+70,6,"V3.1",2);
	
	OLED_ShowCN(0,0,12);//显示光标位置
	
	while(1)
	{
		//OLED_Fill(0xFF);//全屏点亮
		//DelayS(1);
		//OLED_Fill(0x00);//全屏灭
		/*DelayS(1);
		for(i=0;i<5;i++)
		{
			OLED_ShowCN(22+i*16,0,i);//测试显示中文
		}
		DelayS(2);
		OLED_ShowStr(0,3,"HelTec Automation",1);//测试6*8字符
		OLED_ShowStr(0,4,"Hello Tech",2);				//测试8*16字符
		DelayS(2);*/
		
		/*OLED_OFF();//测试OLED休眠
		DelayS(2);
		OLED_ON();//测试OLED休眠后唤醒*/
		//OLED_DrawBMP(0,0,128,8,(unsigned char *)BMP1);//测试BMP位图显示
		//DelayS(3);
		//WriteCmd(0xA0);
		//WriteCmd(0xC0);
		//DelayS(3);
		//OLED_Init();
		if(!S1)
		{
			DelayMs(10);
			if(!S1)	//K1
				
			{
				 while(!S1);//等待按键释放
				 //OLED_ShowStr(3,0,"1111",2);//测试6*8字符;
				//OLED_ShowCN(0,0,0);
				if(edit==0)
				{
					OLED_ShowCN(0,y,14);//清除上个位置的光标

					y=y-2;
					
					if(y<0)
					{
						y=6;
					}
					OLED_ShowCN(0,y,12);//显示光标位置
			  }
				if(edit==1&&y==0)//编辑速度
				{
					speed++;
					if(speed>9)
					{
						speed=9;
					}
					set_speed(speed);
				}
				if(edit==1&&y==2)//编辑时间
				{
					time=time+10;
					if(time>90)
					{
						time=90;
					}
					set_time(time);
				}
			}
		}
		
		////////////////////////////////////////////
		if(!S2)//K2
		{
			DelayMs(10);
			if(!S2)	
				
			{
				 while(!S2);
				 //OLED_ShowStr(3,2,"2222",2);//测试6*8字符;
				if(edit==0)
				{
					 OLED_ShowCN(0,y,14);//清除上个位置的光标
					 y=y+2;
					 if(y>6)
					 {
						 y=0;
					 }
					 OLED_ShowCN(0,y,12);//显示光标位置
			  }
				 if(edit==1&&y==0)//编辑速度
				{
					speed--;
					if(speed<1)
					{
						speed=1;
					}
					set_speed(speed);
				}
				if(edit==1&&y==2)//编辑时间
				{
					time=time-10;
					if(time<10)
					{
						time=10;
					}
					set_time(time);
				}
			}
		}
		//////////////////////////////////////////
		if(!S3)//K3:#编辑
		{
			DelayMs(10);
			if(!S3)	
				
			{
				 while(!S3);
				 //OLED_ShowStr(3,4,"3333",2);//测试6*8字符;;;
				OLED_ShowCN(0,y,13);//清除上个位置的光标
				edit=1;//进入编辑状态
			}
		}
	///////////////////////////////////////////
		if(!S4)//K4:*确认，返回
		{
			DelayMs(10);
			if(!S4)	
				
			{
				 while(!S4);
				 //OLED_ShowStr(3,6,"4444",2);//测试6*8字符;
				OLED_ShowCN(0,y,12);//清除上个位置的光标
				edit=0;//退出编辑状态，返回菜单
				}
			}
		}

	}
