#include "ASL165_Handler.h"
/* ASL164_Handler entry function */
/* pvParameters contains TaskHandle_t */

extern bsp_leds_t g_bsp_leds;

void ASL165_Handler_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);
    static int myPinCounter = 0;
    static int myLoopCounter = 0;
    uint32_t myPinValue;
    bool pinDirection = false;

    /* LED type structure */
    bsp_leds_t leds = g_bsp_leds;

    /* If this board has no LEDs then trap here */
    if (0 == leds.led_count)
    {
        while (1)
        {
            ;                          // There are no LEDs on this board
        }
    }

    /* Holds level to set for pins */
    bsp_io_level_t pin_level = BSP_IO_LEVEL_LOW;

    /* Enable access to the PFS registers. If using r_ioport module then register protection is automatically
     * handled. This code uses BSP IO functions to show how it is used.
     */
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg (ASL165_DATA, IOPORT_CFG_PORT_DIRECTION_OUTPUT);
    R_BSP_PinAccessDisable();    /* Protect PFS registers */

    vTaskDelay (5);

    while (1)
    {
        /* Enable access to the PFS registers. If using r_ioport module then register protection is automatically
         * handled. This code uses BSP IO functions to show how it is used.
         */
        R_BSP_PinAccessEnable();

//
//        /* Update all board LEDs */
//        for (uint32_t i = 0; i < leds.led_count; i++)
//        {
//            /* Get pin to toggle */
//            uint32_t pin = leds.p_leds[i];
//
//            /* Write to this pin */
//            R_BSP_PinWrite((bsp_io_port_pin_t) pin, pin_level);
//        }

        R_BSP_PinWrite((bsp_io_port_pin_t) leds.p_leds[0], pin_level);

        //R_BSP_PinWrite (ASL165_DATA, pin_level);    // P603
        //R_BSP_PinWrite (ASL165_CTS, pin_level);   // P602
        //R_BSP_PinWrite (ASL165_RTS, pin_level);   // P601
        R_BSP_PinWrite (ASL165_CLK, pin_level);   // P600

        ++myLoopCounter;
        myPinValue = R_BSP_PinRead (ASL165_DATA);
        if (myPinValue)
            ++myPinCounter;
        R_BSP_PinWrite((bsp_io_port_pin_t) leds.p_leds[1], myPinValue);

        if ((myLoopCounter & 0x07) == 0)
        {
            if (pinDirection)
            {
                R_BSP_PinCfg (ASL165_DATA, IOPORT_CFG_PORT_DIRECTION_INPUT);
                pinDirection = false;
            }
            else
            {
                R_BSP_PinCfg (ASL165_DATA, IOPORT_CFG_PORT_DIRECTION_OUTPUT);
                pinDirection = true;
            }
        }

        /* Protect PFS registers */
        R_BSP_PinAccessDisable();

        /* Toggle level for next write */
        if (BSP_IO_LEVEL_LOW == pin_level)
        {
            pin_level = BSP_IO_LEVEL_HIGH;
        }
        else
        {
            pin_level = BSP_IO_LEVEL_LOW;
        }

        vTaskDelay (500);
    }
}
