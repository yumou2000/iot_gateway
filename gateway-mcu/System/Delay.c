#include "stm32f10x.h"                  // Device header

/* 忙等延时基于 DWT 周期计数器（CYCCNT）实现，绝不操作 SysTick 寄存器：
 * FreeRTOS 的 tick 时钟正是 SysTick（port.c 仅在启动时配置一次，
 * SysTick_Handler 不会重新使能）。若像旧实现那样改写 SysTick->LOAD/VAL/CTRL，
 * 并在延时结束后把 CTRL 写成 0x04（ENABLE=0），会永久关闭 SysTick，
 * 导致 FreeRTOS tick 中断停止、所有 vTaskDelay 挂起、整个系统"半死"。
 * 这正是 MQTT 模式下运行一段时间后设备不再响应/上报的根源之一。 */
static void Delay_DWT_Init(void)
{
	if(!(DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk))
	{
		CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk; /* 使能 DWT 访问 */
		DWT->CYCCNT = 0;
		DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;            /* 启动周期计数器 */
	}
}

/**
	延迟us的函数（DWT 忙等，72MHz 下 1us = 72 个周期）
*/
void Delay_us(uint32_t us)
{
	Delay_DWT_Init();
	uint32_t start = DWT->CYCCNT;
	uint32_t ticks = us * (SystemCoreClock / 1000000);
	/* 无符号减法处理计数器回绕（约 59.6s 回绕一次） */
	while((DWT->CYCCNT - start) < ticks);
}

/**
	延迟ms的函数
*/
void Delay_ms(uint32_t ms)
{
	while(ms--)
	{
		Delay_us(1000);
	}
}

/**
	延迟s的函数
*/
void Delay_s(uint32_t s)
{
	while(s--)
	{
		Delay_ms(1000);
	}
}
