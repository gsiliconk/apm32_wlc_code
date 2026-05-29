#include "bsp_timer.h"

static void (*g_timer_callback)(void) = NULL;

void bsp_timer_set_callback(void (*cb)(void))
{
	g_timer_callback = cb;
}

void bsp_timer_init(void)
{
    TMR_BaseConfig_T timer;
    RCM_EnableAPB1PeriphClock(RCM_APB1_PERIPH_TMR2);

    timer.clockDivision     = TMR_CLOCK_DIV_1;
    timer.countMode         = TMR_COUNTER_MODE_UP;
    timer.division          = 71;        /* 72MHz / 72 = 1MHz */
    timer.period            = 999;       /* 1MHz / 1000 = 1kHz ¡ú 1ms */
    timer.repetitionCounter = 0;
    TMR_ConfigTimeBase(TMR2, &timer);

    TMR_EnableInterrupt(TMR2, TMR_INT_UPDATE);
    NVIC_EnableIRQRequest(TMR2_IRQn, 4, 2);
    TMR_Enable(TMR2);
}

void TMR2_IRQHandler(void)
{
    if(TMR_ReadIntFlag(TMR2, TMR_INT_UPDATE) == SET)
		{
        TMR_ClearIntFlag(TMR2, TMR_INT_UPDATE);
        if(g_timer_callback)
					g_timer_callback();
    }
}