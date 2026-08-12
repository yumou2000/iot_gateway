#include "stm32f10x.h"                  // Device header

//LED1---PA0     LED2--PA1
void LED_Init(void)
{
	RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA,ENABLE);
	
	GPIO_InitTypeDef GPIO_InitStruct;
	GPIO_InitStruct.GPIO_Mode = GPIO_Mode_Out_PP;
	GPIO_InitStruct.GPIO_Pin =GPIO_Pin_2 ;
	GPIO_InitStruct.GPIO_Speed = GPIO_Speed_50MHz;
	GPIO_Init(GPIOA,&GPIO_InitStruct);

	//初始化的时候，灯都是灭的
	GPIO_ResetBits(GPIOA,GPIO_Pin_2);
}
//点亮LED1
void LED1_ON(void)
{
	GPIO_SetBits(GPIOA,GPIO_Pin_2 );
}

void LED1_OFF(void)
{
	GPIO_ResetBits(GPIOA,GPIO_Pin_2 );
}

void LED1_Toggle(void)
{
	//读取A0引脚当前的状态
	 if(GPIO_ReadOutputDataBit(GPIOA,GPIO_Pin_2))
	 {
		LED1_OFF();
	 }
	 else
	 {
		LED1_ON();
	 }
}
	
void LED2_ON(void)
{
	GPIO_SetBits(GPIOA,GPIO_Pin_1 );
}

void LED2_OFF(void)
{
	GPIO_ResetBits(GPIOA,GPIO_Pin_1 );
}

void LED2_Toggle(void)//翻转电平
{
	//读取A0引脚当前的状态
	 if(GPIO_ReadOutputDataBit(GPIOA,GPIO_Pin_1))
	 {
		LED2_OFF();
	 }
	 else
	 {
		LED2_ON();
	 }
}


