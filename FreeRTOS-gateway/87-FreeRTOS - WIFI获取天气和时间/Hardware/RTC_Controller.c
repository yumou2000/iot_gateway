#include "stm32f10x.h"                  // Device header
#include "RTC_Controller.h"

//初始化RTC时钟，作为系统时钟

void RTC_CTRL_Init(void){
	//======注意1.对后备寄存器的访问是被禁止的
	// 所以使能 PWR 和 BKP 时钟，这是访问后备域的前提
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_BKP | RCC_APB1Periph_PWR , ENABLE);
	PWR_BackupAccessCmd(ENABLE);
	
	//如果之前设置过值，那就只继续同步就行，没设置过就会执行else语句
	if(RTC_GetCounter() > 0){
		//该函数会等待直到同步完成，确保后续读取 RTC 寄存器值的准确性
		RTC_WaitForSynchro();
		
	}else{
		//1.开启动LSE
		RCC_LSEConfig(RCC_LSE_ON);
		while(RCC_GetFlagStatus(RCC_FLAG_LSERDY) == RESET);
		
		//2.LSE选择RTC时钟
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);
		RCC_RTCCLKCmd(ENABLE);
		
		//======注意2.APBI时钟和RTC时钟要同步一次，APB1管写入PSC和CNT
		RTC_WaitForSynchro();//已经写好的函数
		
		/* 注意3: RTC要进入配置模式，才可以设置RTC_PRL、RTC_CNT、RTC_ALR
		  下面两个函数的原码里，都有写了，所以不用写 	*/
		
		//3.配置RTC时钟(预分频和计数器)
		//注意4.等待上一次操作完成
		RTC_WaitForLastTask();
		RTC_SetPrescaler(32768 - 1);
		RTC_WaitForLastTask();
		RTC_SetCounter(0);
		RTC_WaitForLastTask();
	}
}
	
//设置闹钟
void setAlarm(uint32_t alarmValue){
	
	RTC_SetAlarm(alarmValue);//设置闹钟的值
	
	RTC_WaitForLastTask();//等待前一次执行结束
}


void setCounter(int32_t counter){
	RTC_WaitForLastTask();
	RTC_SetCounter(counter);
	
	RTC_WaitForLastTask();
}












