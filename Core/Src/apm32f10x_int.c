#include "apm32f10x_int.h"

//void SVC_Handler(void)
//{
//    // FreeRTOS接管，不需要手动处理
//}

//void PendSV_Handler(void)
//{
//    // FreeRTOS接管，不需要手动处理
//}

//void SysTick_Handler(void)
//{
//    // FreeRTOS接管，不需要手动处理
//}

void NMI_Handler(void)
{
}

void HardFault_Handler(void)
{
    while (1);
}

void MemManage_Handler(void)
{
    while (1);
}

void BusFault_Handler(void)
{
    while (1);
}

void UsageFault_Handler(void)
{
    while (1);
}

void DebugMon_Handler(void)
{
}