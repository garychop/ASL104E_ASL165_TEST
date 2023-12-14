#include "FreeRTOSConfig.h"
#include "ASL165_Handler.h"
#include "common_data.h"
#include "./Programmer/ha_hhp_interface_bsp.h"

/* ASL164_Handler entry function */
/* pvParameters contains TaskHandle_t */

//extern bsp_leds_t g_bsp_leds;

//-----------------------------------------------------------------------------
// Local Macros

#define HHP_RX_TX_BUFF_LEN (8)  // Length of the HHP communications rx/tx buffer

typedef enum
{
    FUNC_FEATURE_POWER_ON_OFF,
    FUNC_FEATURE_OUT_CTRL_TO_BT_MODULE,
    FUNC_FEATURE_OUT_NEXT_FUNCTION,
    FUNC_FEATURE_OUT_NEXT_PROFILE,
    FUNC_FEATURE_RNET_SEATING,
    FUNC_FEATURE2_RNET_SLEEP,
    // Nothing else may be defined past this point!
    FUNC_FEATURE_EOL
} FunctionalFeature_t;

typedef enum
{
    HA_HHP_CMD_PAD_ASSIGNMENT_GET = 0x30,
    HA_HHP_CMD_PAD_ASSIGNMENT_SET = 0x31,
    HA_HHP_CMD_CAL_RANGE_GET = 0x32,
    HA_HHP_CMD_CAL_RANGE_SET = 0x33,
    HA_HHP_CMD_PAD_CALIBRATION_SESSION_START = 0x34,
    HA_HHP_CMD_PAD_CALIBRATION_SESSION_STOP = 0x35,
    HA_HHP_CMD_PAD_DATA_GET = 0x36,
    HA_HHP_CMD_VERSION_GET = 0x37,
    HA_HHP_CMD_ENABLED_FEATURES_GET = 0x38,
    HA_HHP_CMD_ENABLED_FEATURES_SET = 0x39,
    HA_HHP_CMD_MODE_OF_OPERATION_SET = 0x3A,
    HA_HHP_CMD_HEARTBEAT = 0x3B,
    HA_HHP_CMD_NEUTRAL_DAC_GET = 0x3C,
    HA_HHP_CMD_NEUTRAL_DAC_SET = 0x3D,
    HA_HHP_CMD_SAVE_PARAMETERS = 0x3E,
    HA_HHP_CMD_RESET_PARAMETERS = 0x3F,
    HA_HHP_CMD_DRIVE_OFFSET_GET = 0x40,
    HA_HHP_CMD_DRIVE_OFFSET_SET = 0x41,
    HA_HHP_CMD_ATTENDANT_SETTINGS_GET = 0x42,
    HA_HHP_CMD_ATTENDANT_SETTINGS_SET = 0x43,
    HA_HHP_CMD_ATTENDANT_CONTROL = 0x44
} HaHhpIfCmd_t;

// Slave responses to commands from master.
typedef enum
{
    HA_HHP_RESP_ACK = 0x06,
    HA_HHP_RESP_NACK = 0x15
} HaHhpIfResp_t;

//-----------------------------------------------------------------------------
// Local / Global variables
static uint8_t hhp_rx_data_buff[HHP_RX_TX_BUFF_LEN];
static uint8_t hhp_tx_pkt_buff[HHP_RX_TX_BUFF_LEN];
uint8_t g_CurrentHeartBeat = 0;

//-----------------------------------------------------------------------------
// Forward Declarations
bool haHhp_RxPacket(uint8_t *rx_buff);
static void ProcessRxdPacket(uint8_t *rxd_pkt, uint8_t *pkt_to_tx);
static void BuildAckPacket(uint8_t *pkt_to_tx);
static void BuildNackPacket(uint8_t *pkt_to_tx);
static uint8_t CalcChecksum(uint8_t *packet, uint8_t len);
static void BuildHeartBeatResponsePacket(uint8_t *rxd_pkt, uint8_t *pkt_to_tx);

static void BuildEnabledFeaturesResponsePacket(uint8_t *pkt_to_tx);

uint8_t g_external_irq_complete = 0;

//-----------------------------------------------------------------------------
/* Called from icu_irq_isr */
void HHP_MasterRTS_Interrupt (external_irq_callback_args_t * p_args)
{
    (void) p_args;
    g_external_irq_complete = 1;
}

//-----------------------------------------------------------------------------
// This is the HHP Interface Task.

void ASL165_Handler_entry(void *pvParameters)
{
    FSP_PARAMETER_NOT_USED (pvParameters);

    /* Enable access to the PFS registers. If using r_ioport module then register protection is automatically
     * handled. This code uses BSP IO functions to show how it is used.
     */
    R_BSP_PinAccessEnable();
    R_BSP_PinCfg (ASL165_DATA, IOPORT_CFG_PORT_DIRECTION_INPUT);
    R_BSP_PinCfg (ASL165_CLK, IOPORT_CFG_PORT_DIRECTION_INPUT);
//    R_BSP_PinCfg (ASL165_RTS, IOPORT_CFG_PORT_DIRECTION_INPUT);
    R_BSP_PinCfg (ASL165_CTS, IOPORT_CFG_PORT_DIRECTION_OUTPUT);
//    R_BSP_PinAccessDisable();    /* Protect PFS registers */

    vTaskDelay (5);

    R_BSP_PinAccessEnable();

    /* Configure the external interrupt. */
    fsp_err_t err = R_ICU_ExternalIrqOpen(&g_external_irq0_ctrl, &g_external_irq0_cfg);
    assert(FSP_SUCCESS == err);
    /* Enable the external interrupt. */
    /* Enable not required when used with ELC or DMAC. */
    err = R_ICU_ExternalIrqEnable(&g_external_irq0_ctrl);
    assert(FSP_SUCCESS == err);

    while (1)
    {
        if (g_external_irq_complete)
        //if (haHhpBsp_MasterRtsAsserted())
        {
            g_external_irq_complete = 0;
            if (haHhpBsp_MasterRtsAsserted()) // This is really not necessary but I put this in too ensure that the ASL165 is ready.
            {
                // Receive, process, and respond to the packet without fear of OS ticks in
                // and mucking with timing.  Note: sys tick is active, but only processes the
                // absolute essentials.
                //GC isrsOsTickEnable(false); // Essentially, disable interrupts.

                if (haHhpBsp_ReadyToReceivePacket())
                {
                    haHhpBsp_SlaveReadyToReceivePacket();

                    if(haHhp_RxPacket(hhp_rx_data_buff))
                    {
                        ProcessRxdPacket(hhp_rx_data_buff, hhp_tx_pkt_buff);

                        haHhpBsp_TransmitPacket(hhp_tx_pkt_buff, hhp_tx_pkt_buff[0]);
                    }
                }
            //GC isrsOsTickEnable(true);
            }
        }

        // Need to keep the wait very short, but need to wait.
        //task_wait(MILLISECONDS_TO_TICKS(1));

        vTaskDelay (2);
    }
}


#ifdef USING_ORIGINAL_CODE

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

    R_BSP_PinAccessEnable();

    while (1)
    {
        /* Enable access to the PFS registers. If using r_ioport module then register protection is automatically
         * handled. This code uses BSP IO functions to show how it is used.
         */
//        R_BSP_PinAccessEnable();

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
//        R_BSP_PinAccessDisable();

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

#endif // #ifdef USING_ORIGINAL_CODE

//-------------------------------
// Function: haHhp_RxPacket
//
// Description: Reads in an ensure packet from over the HHP communications interface.
//
// return: true: read in packet, false: timed out trying to read in packet.
//-------------------------------

bool haHhp_RxPacket(uint8_t *rx_buff)
{
    bool ret_val = true;

    // Receive the length of the packet
    if (haHhpBsp_RxByte(&rx_buff[0]) &&
        (rx_buff[0] <= HHP_RX_TX_BUFF_LEN))
    {
        // Get the rest of the packet.
        // Start at 1 because the message length byte itself is in the packet and required for the checksum calc
        for (unsigned int i = 1; (i < rx_buff[0]) && ret_val; i++)
        {
            ret_val = haHhpBsp_RxByte(&rx_buff[i]);
        }
        ret_val = true;
    }
    else
    {
        // Message is too long or timed out.
        ret_val = false;
    }

    return ret_val;
}


//-------------------------------
// Function: ProcessRxdPacket
//
// Description: Processes a received packet from a master device (HHP)
//
// Basic command->response format:
// Request from master: <LEN><DATA><CHKSUM>
// Response from slave: <LEN><DATA><CHKSUM>
//
//-------------------------------

uint16_t myHB_Counter = 0;

static void ProcessRxdPacket(uint8_t *rxd_pkt, uint8_t *pkt_to_tx)
{
//    uint8_t myData[8];

    // Don't include the checksum value when computing the checksum.
    // If this dewvice is in idle mode, then command processing outside of heartbeat is disabled.
//  if ((AppCommonDeviceActiveGet() || (!AppCommonDeviceActiveGet() && (HA_HHP_CMD_HEARTBEAT == (HaHhpIfCmd_t)rxd_pkt[1]))) &&
//      (rxd_pkt[0] <= HHP_RX_TX_BUFF_LEN) &&
//      (CalcChecksum(rxd_pkt, rxd_pkt[0] - 1) == rxd_pkt[rxd_pkt[0] - 1]))
    if (CalcChecksum(rxd_pkt, rxd_pkt[0] - 1) == rxd_pkt[rxd_pkt[0] - 1])
    {
        // At a basic structural level, the packet is constructed properly.
        switch((HaHhpIfCmd_t)rxd_pkt[1])
        {
            // Gets pad left/right/forward/backwards, and proportional/digital input assignments
            case HA_HHP_CMD_PAD_ASSIGNMENT_GET:
                // Received packet structure:
                // <LEN><PAD_ASSIGNMENT_GET_CMD><PHYSCIAL PAD><CHKSUM>
                //
                // Response packet structure:
                // <LEN><LOGICAL PAD><SENSOR TYPE><CHKSUM>
                // or
                // <LEN><NACK><CHKSUM>
                //
                // Where:   <PAD_ASSIGNMENT_GET> = 0x30
                //          <PHYSICAL PAD> = "L", "R", "C"
                //          <LOGICAL PAD> = "F", "R", "L", "B", "O"
                //          <SENSOR TYPE> = "D", "P"
//GC                HandlePadAssignmentGet(rxd_pkt, pkt_to_tx);
                break;

            // Sets pad left/right/forward/backwards, and proportional/digital input assignments
            case HA_HHP_CMD_PAD_ASSIGNMENT_SET:
                // Received packet structure:
                // <LEN><PAD_ASSIGNMENT_SET_CMD><PHYSCIAL_PAD><LOGICAL_PAD><SENSOR TYPE><CHKSUM>
                //
                // Response packet structure:
                // <LEN><ACK/NACK><CHKSUM>
                //
                // Where:   <PAD_ASSIGNMENT_SET_CMD> = 0x31
                //          <PHYSICAL_PAD> = "L", "R", "C"
                //          <LOGICAL_PAD> = "F", "R", "L", "B", "O"
                //          <SENSOR TYPE> = "D", "P"
                //          <ACK/NAK> = ACK if all is good, NACK if packet is malformed on reception.
//GC                HandlePadAssignmentSet(rxd_pkt, pkt_to_tx);
                BuildAckPacket(pkt_to_tx);
                break;

            // Sets a pad's input min/max raw input value as well as the min/max threshold values for input->output translation.
            case HA_HHP_CMD_CAL_RANGE_GET:
                // Received packet structure:
                // <LEN><CAL_RANGE_GET_CMD><PHYSICAL_PAD><CHKSUM>
                //
                // Response packet structure:
                // <LEN><PHYSICAL_PAD><MIN_ADC><MAX_ADC><MIN_THRESH><MAX_THRESH><CHKSUM>
                // or
                // <LEN><NACK><CHKSUM>
                //
                // Where:   <CAL_RANGE_GET_CMD> = 0x32
                //          <PHYSICAL_PAD> = "L", "R" or "C"
                //          <MIN_ADC> = A value in the range [0,1023].
                //          <MAX_ADC> = A value in the range [0,1023].
                //          <MIN_TRESH> = A value in the range [0,1023].
                //          <MAX_THRESH> = A value in the range [0,1023].
//GC                HandlePadCalibrationDataGet(rxd_pkt, pkt_to_tx);
                break;

            // Sets a pad's input min/max raw input value as well as the min/max threshold values for input->output translation.
            case HA_HHP_CMD_CAL_RANGE_SET:
                // Received packet structure:
                // <LEN><CAL_RANGE_SET_CMD><PHYSICAL_PAD><MIN_THRESH><MAX_THRESH><CHKSUM>
                //
                // Response packet structure:
                // <LEN><ACK/NACK><CHKSUM>
                //
                // Where:   <CAL_RANGE_SET> = 0x33
                //          <PHYSICAL_PAD> = "L", "R" or "C"
                //          <MIN_TRESH> = A value in the range [0,1023].
                //          <MAX_THRESH> = A value in the range [0,1023].
                //          <ACK/NAK> = ACK if all is good, NACK if packet is malformed on reception.
//GC                HandlePadCalibrationDataSet(rxd_pkt, pkt_to_tx);
                BuildAckPacket(pkt_to_tx);
                break;

            // Start calibration session
            case HA_HHP_CMD_PAD_CALIBRATION_SESSION_START:
                // Received packet structure:
                // <LEN><CAL_START_CMD><CHKSUM>
                //
                // Response packet structure:
                // <LEN><ACK/NAK><CHKSUM>
                //
                // Where:   <CAL_START_CMD> = 0x34
                //          <ACK/NAK> = ACK if all is good, NACK if packet is malformed on reception.
//GC                AppCommonCalibrationActiveSet(true);
                BuildAckPacket(pkt_to_tx);
                break;

            // Stop calibration session
            case HA_HHP_CMD_PAD_CALIBRATION_SESSION_STOP:
                // Received packet structure:
                // <LEN><CAL_STOP_CMD><CHKSUM>
                //
                // Response packet structure:
                // <LEN><ACK/NAK><CHKSUM>
                //
                // Where:   <CAL_STOP_CMD> = 0x35
                //          <ACK/NAK> = ACK if all is good, NACK if packet is malformed on reception.

//GC                AppCommonCalibrationActiveSet(false);
                BuildAckPacket(pkt_to_tx);
                break;

            // Get pad related information
            case HA_HHP_CMD_PAD_DATA_GET:
                // Received packet structure:
                // <LEN><PAD_DATA_GET_CMD><PHYSICAL PAD><CHKSUM>
                //
                // Response packet structure:
                // <LEN><RAW DATA><ADJUSTED DATA><CHKSUM>
                // or
                // <LEN><NACK><CHKSUM>
                // Where:   <PAD_DATA_GET_CMD> = 0x36
                //          <PHYSICAL PAD> = "L", "R", "C"
                //          <RAW DATA> = the actual analog/digital raw sensor data
                //          <ADJUSTED DATA> = the processed Joystick Demand.
//GC                HandlePadDataGet(rxd_pkt, pkt_to_tx);
                break;

            // Gets version of the head array firmware.
            case HA_HHP_CMD_VERSION_GET:
                // Received packet structure:
                // <LEN><VERSIONS_GET_CMD><CHKSUM>
                //
                // Response packet structure:
                // <LEN><MAJOR><MINOR><BUILD><DATA_VERSION><CHKSUM>
                // or
                // <LEN><NACK><CHKSUM>
                //
                // Where:   <REQUEST_VERSIONS_CMD> = 0x37
                //          <MAJOR>, <MINOR>, <BUILD> Firmware version of head array represented as binary numbers.
                //          <DATA VERSION> is the version of EEPROM data packing definition.
//GC                BuildVersionsResponsePacket(pkt_to_tx);
                break;

            // Get currently enabled features of the head array.
            case HA_HHP_CMD_ENABLED_FEATURES_GET:
                // Received packet structure:
                // <LEN><FEATURES_GET_CMD><CHKSUM>
                //
                // Response packet structure:
                // <LEN><FEATURE_BITS><LONG_PRESS_TIME><CHKSUM>
                //
                // Where:   <REQUEST_FEATURE_SETTING_CMD> = 0x38
                //          <FEATURE_BITS> is a bit pattern as follows:
                //              0x01 is POWER ON/OFF setting where 1 is enabled, 0 is disabled.
                //              0x02 is BLUETOOTH where 1 is enabled, 0 is disabled.
                //              0x04 is NEXT FEATURE where 1 is enabled, 0 is disabled.
                //              0x08 is NEXT PROFILE where 1 is enabled, 0 is disabled.
                //              0x10 is the SOUND setting where 1 is enabled, 0 is disabled.
                //              0x20 is the POWER UP IN IDLE setting where 1 is enabled, 0 is disabled.
                //              0x40 is the RNet MENU setting where 1 is enabled, 0 is disabled.
                //          <LONG_PRESS_TIME> is length of time for a Long Press of the Mode Switch in tenths of seconds. "15" is 1.5 seconds.
                //          <FEATURE_BITS_2>
                //              0x01 is RNET_SLEEP, 1=enabled, 0=disabled.
                //              0x02 is MODE_REVERSE, 1=enabled, 0=disabled (Normal Pin5 operation)
                //            0x04 is MAIN SCREEN PAD SENSOR display, 1=enabled, 0=disabled.
                //            0x08 through 0x80 are undefined.
                BuildEnabledFeaturesResponsePacket(pkt_to_tx);
                break;

            case HA_HHP_CMD_ENABLED_FEATURES_SET:
                // Received packet structure:
                // <LEN><FEATURES_SET_CMD><FEATURE_BITS><LONG_PRESS_TIME><CHKSUM>
                //
                // Response packet structure:
                // <LEN><ACK/NAK><CHKSUM>
                // Where:   <FEATURES_SET_CMD> = 0x39
                //          <FEATURE_BITS> is a bit pattern as follows:
                //              0x01: POWER ON/OFF, 1=enabled, 0=disabled.
                //              0x02: BLUETOOTH, 1=enabled, 0=disabled.
                //              0x04: NEXT FEATURE, 1=enabled, 0=disabled.
                //              0x08: NEXT PROFILE, 1=enabled, 0=disabled.
                //              0x10: SOUND ENABLED, 1=enabled, 0=disabled.
                //              0x20 is the POWER UP IN IDLE setting where 1 is enabled, 0 is disabled.
                //              0x40 is the RNet MENU setting where 1 is enabled, 0 is disabled.
                //          <LONG_PRESS_TIME> is length of time for a Long Press of the Mode Switch in tenths of seconds.
                //                  "15" is 1.5 seconds.
                //        <FEATURE_BITS_2>
                //            0x01 is RNET_SLEEP, 1=enabled, 0=disabled.
                //            0x02 is MODE REVERSE, 1=enabled, 0=disabled.
                //            0x04 is MAIN SCREEN PAD SENSOR display, 1=enabled, 0=disabled.
                //            0x08 through 0x80 are undefined.
                //          <ACK/NAK> = ACK if all is good, NACK if packet is malformed on reception.
//GC                HandleEnabledFeaturesSet(rxd_pkt, pkt_to_tx);
                BuildAckPacket(pkt_to_tx);
                break;

            // Set mode of operation of head array.
            case HA_HHP_CMD_MODE_OF_OPERATION_SET:
                // Received packet structure:
                // <LEN><ACTIVE_FEATURE_SET_CMD><ACTIVE_FEATURE><CHKSUM>
                //
                // Response packet structure:
                // <LEN><ACK/NAK><CHKSUM>
                // Where:   <ACTIVE_FEATURE_SET_CMD> = 0x3A
                //          <ACTIVE_FEATURE> = May be one of the following values:
                //              0x01: POWER ON/OFF (puts head array into idle/ready modes of operation)
                //              0x02: BLUETOOTH
                //              0x03: NEXT FUNCTION
                //              0x04: NEXT PROFILE
                //              0x05: RNet DRIVING
                //              0x06: RNet SEATING.
                //          <ACK/NAK> = ACK if all is good, NACK if packet is malformed on reception or profile disabled.
                //
                // WARN: If the values for the above functions ever change, then the values of FunctionalFeature_t
                // WARN: must be changed to match!
//GC                HandleModeOfOpSet(rxd_pkt, pkt_to_tx);
                break;

            case HA_HHP_CMD_HEARTBEAT:
                // This is the "link status check" that is maintained by the master.
                //
                // Keeps the communications bus "alive". The master must know that this device
                // is alive, this "heartbeat" command is sent periodically to determine just that.
                //
                // Received packet structure:
                // <LEN><HEARTBEAT_CMD><VALUE><CHKSUM>
                // <LEN><VALUE><ACTIVE_FEATURE><STATUS><CHKSUM>
                // or
                // <LEN><NACK><CHKSUM>
                // Where:   <HEARTBEAT_CMD> = 0x3B
                //          <VALUE> = An increment byte value from 0 through 0xff. The response echoes the same <VALUE>.
                //          <ACTIVE_FEATURE> = Current active feature responding with one of the following values:
                //            0x01 for POWER ON/OFF (puts head array into idle/ready modes of operation)
                //            0x02 for BLUETOOTH
                //            0x03 for NEXT FUNCTION
                //            0x04 for NEXT PROFILE
                //            0x05 for RNet MENU SEATING
                //            0x06 for RNet SLEEP
                //          <STATUS> = System status of the head array that is a bit pattern as follows:
                //              0x01: Head array is in idle/ready mode. 0=idle, 1=ready
                //              0x02: Head array is in calibration mode. 0=normal, 1=calibration
                //              0x04: left pad connected/disconnected status. 0=disconnected, 1=connected
                //              0x08: right pad connected/disconnected status. 0=disconnected, 1=connected
                //              0x10: center pad connected/disconnected status. 0=disconnected, 1=connected
                //              0x20: Out of neutral failure (goes away once user stops pressing all pads for a
                //                      sufficient amount of time)
                //              0x40: User Port Switch is active
                //              0x80: Mode Port Switch is active
                //          <STATUS_2> = Additional System Status
                //              0x01: Center Pad Proximity Sensor Active
                //              0x02: Center Pad Pressure Sensor Active
                //              0x04: Right Pad Proximity Sensor Active
                //              0x08: Right Pad Pressure Sensor Active
                //              0x10: Left Pad Proximity Sensor Active
                //              0x20: Left Pad Pressure Sensor Active
                BuildHeartBeatResponsePacket(rxd_pkt, pkt_to_tx);
                ++myHB_Counter;
                break;
            case HA_HHP_CMD_NEUTRAL_DAC_GET:
                //    Neutral DAC Get Command (0x3c
                //)
                //    <LEN><NEUTRAL_DAC_GET_CMD><CHKSUM>
                //    <LEN><NEUTRAL_CONSTANT><NEUTRAL_DAC_VALUE><DAC_RANGE><CHKSUM>
                //    or
                //    <LEN><NACK><CHKSUM>
                //    Where:    <NEUTRAL_DAC_GET_CMD> = 0x3c
                //              <NUETRAL_CONSTANT> = A value in the range [0,4096], typical 2048
                //              <NUETRAL_DAC_VALUE> = A value in the range [0,4096], typical 2048
                //              <DAC_RANGE> = A value in the range [0,4096], typical 410
//GC                Buid_DAC_Get_Response(rxd_pkt, pkt_to_tx);
                break;

            case HA_HHP_CMD_NEUTRAL_DAC_SET:
                //    Neutral DAC Set Command (0x3d)
                //
                //    <LEN><NEUTRAL_DAC_SET_CMD><NEUTRAL_DAC_VALUE><CHKSUM>
                //    <LEN><NCK><CHKSUM>
                //    or
                //    <LEN><NACK><CHKSUM>
                //    Where:    <NUETRAL_DAC_SET_CMD> = 0x3d
                //              <NUETRAL_DAC_VALUE> = A value in the range [0,4096], typical 2048
//GC                Handle_DAC_SetCommand (rxd_pkt, pkt_to_tx);
                break;

            case HA_HHP_CMD_SAVE_PARAMETERS:
                // Save Parameter Command (0x3e)
                //
                //  <LEN><SAVE_PARAMETERS_CMD><CHKSUM>
                //  <LEN><ACK><CHKSUM>
//GC                eepromFlush (true);      // save all parameters.
                BuildAckPacket(pkt_to_tx);
                break;

            case HA_HHP_CMD_RESET_PARAMETERS:
                // Reset Parameter Command (0x3f)
                //
                //  <LEN><RESET_PARAMETERS_CMD><CHKSUM>
                //  <LEN><ACK><CHKSUM>
//GC                SetDefaultValues();
//GC                eepromFlush (true);      // save all parameters.
                BuildAckPacket(pkt_to_tx);
                break;

            case HA_HHP_CMD_DRIVE_OFFSET_GET:
                // Get the Drive Offset Value
                //
                //  <LEN><DRIVE_OFFSET_GET_CMD><CHKSUM>
                //  <LEN><OFFSET_VALUE><CHKSUM>
//GC                CreateGetOffsetResponse (pkt_to_tx);
                break;

            case HA_HHP_CMD_DRIVE_OFFSET_SET:
                // Set the Drive Offset Value
                //
                //  <LEN><DRIVE_OFFSET_SET_CMD><OFFSET_VALUE><CHKSUM>
                //  <LEN><ACK><CHKSUM>
//GC                CreateSetOffsetResponse (rxd_pkt, pkt_to_tx);
                break;

            case HA_HHP_CMD_ATTENDANT_SETTINGS_GET:
                //Receive: <LEN>< ATTENDANT_SETTINGS_GET_CMD><CHKSUM>
                //Respond: <LEN><SETUP><TIMEOUT><CHKSUM>
                //Where:    <ATTENDANT_SETTINGS_GET_CMD> = 0x42
                //  <SETUP>
                //      DO: 0 = Attendant control disabled, 1 = Attendant control enabled.
                //      D1: 0 = Proportional Control, 1 = Digital Control
                //      D2: 0 = Use as Assist to Patient, 1 = Override Patent demands
                //      D3: 0 = Portrait orientation, 1 = Landscape.
                //  <TIMEOUT>
                //      0 ? 127 seconds, 0 = No timeout
//GC                CreateGetAttendantSettingsResponse (pkt_to_tx);
                break;

            case HA_HHP_CMD_ATTENDANT_SETTINGS_SET:
                //<LEN>< ATTENDANT_SETTINGS_SET_CMD ><SETUP><TIMEOUT><CHKSUM>
                //<LEN><ACK/NACK><CHKSUM>
                //Where:    <ATTENDANT_SETTINGS_SET_CMD> = 0x43
                //      See ?GET? command for parameters definitions
//GC                CreateSetAttendantSettingsResponse (rxd_pkt, pkt_to_tx);
                break;

            case HA_HHP_CMD_ATTENDANT_CONTROL:
                //<LEN><ATTENDANT_CONTROL_CMD><ACTIVE><SPEED><DIRECTION><HEARTBEAT><CHKSUM>
                //<LEN><ACK/NACK><CHKSUM>
                //Where:    <ATTENDANT_CONTROL_CMD> = 0x46
                //  <ACTIVE> is 1 if Attendant Control is active in ASL165 Display.
                //  <SPEED> is a -100 (reverse) to 100 (forward) value where 0 is Neutral.
                //  <DIRECTION> is a -100 (left) to 100 (right) value where 0 is Neutral.
                //  <HEARTBEAT> is a Heart Beat value incremented on each transmission.
//GC                g_AttendantActive = rxd_pkt[2];
//GC                g_AttendantSpeedDemand = rxd_pkt[3];
//GC                g_AttendantDirectionDemand = rxd_pkt[4];
//GC                g_AttendantActiveHeartBeatCounter = 0;  // Clear the keep alive counter;
                break;

            default:
//                myData[0] = *rxd_pkt;
//                myData[1] = *(rxd_pkt+1);
//                myData[2] = *(rxd_pkt+2);
//                myData[3] = *(rxd_pkt+3);
//                myData[4] = *(rxd_pkt+4);
                BuildNackPacket(pkt_to_tx);
                break;
        }
    }
    else
    {
        // Packet is not constructed properly. Try to send a NACK back to the master device.
        BuildNackPacket(pkt_to_tx);
    }

    pkt_to_tx[pkt_to_tx[0] - 1] = CalcChecksum(pkt_to_tx, pkt_to_tx[0] - 1);
}

//-------------------------------
// Function: BuildHeartBeatResponsePacket
//
// Description: Builds up a packet to send as a response to a HA_HHP_CMD_VERSION_GET command.
//
// NOTE: The current mapping for values received in rxd_pkt[2] and FunctionalFeature_t is a simple offset of 1.
//
//-------------------------------
static void BuildHeartBeatResponsePacket(uint8_t *rxd_pkt, uint8_t *pkt_to_tx)
{
// wow. I've never put defines in a function before.
#define PROP_MIN 0x0

    FunctionalFeature_t currentFeature;
    //uint8_t switchStatus;

    pkt_to_tx[0] = 6;                       // 1-based message length
    pkt_to_tx[1] = rxd_pkt[2];              // xfer the Command ID
    g_CurrentHeartBeat = rxd_pkt[2];        // Store for use to determine if Display is still
                                            // connected. Specifically for Attendant control.
    currentFeature = FUNC_FEATURE_POWER_ON_OFF; // eepromEnumGet(EEPROM_STORED_ITEM_CURRENT_ACTIVE_FEATURE);
    pkt_to_tx[2] = ++currentFeature;    // Make it base 1

    // Build up status byte
    pkt_to_tx[3] = 0x00;
    pkt_to_tx[3] |= 0x01; // AppCommonDeviceActiveGet() ? 0x01 : 0x00;
    //pkt_to_tx[3] |= AppCommonCalibrationActiveGet() ? 0x02 : 0x00;
    pkt_to_tx[3] |= 0x04; // headArrayPadIsConnected(HEAD_ARRAY_SENSOR_LEFT) ? 0x04 : 0x00;
    pkt_to_tx[3] |= 0x08; // headArrayPadIsConnected(HEAD_ARRAY_SENSOR_RIGHT) ? 0x08 : 0x00;
    pkt_to_tx[3] |= 0x10; // headArrayPadIsConnected(HEAD_ARRAY_SENSOR_CENTER) ? 0x10 : 0x00;
    pkt_to_tx[3] |= 0x00; // headArrayNeutralTestFail() ? 0x20 : 0x00;
    //switchStatus = 0x00; // GetSwitchStatus ();
    pkt_to_tx[3] |= 0x00; // (switchStatus & USER_SWITCH) ? 0x040 : 0x00;
    pkt_to_tx[3] |= 0x00; // (switchStatus & MODE_SWITCH) ? 0x080 : 0x00;

    // Set Proximity and Pressure Sensor status.
    // Send the status regardless of the feature setting, but if you must here's the code
    //if ((eeprom8bitGet(EEPROM_STORED_ITEM_ENABLED_FEATURES_2) & FUNC_FEATURE2_SHOW_PADS_BIT_MASK) > 0)
    pkt_to_tx[4] = 0x00;
//    pkt_to_tx[4] = headArrayBspDigitalState(HEAD_ARRAY_SENSOR_CENTER) ? 0x01 : 0x00;
//    if (g_PadProportionalValue[HEAD_ARRAY_SENSOR_CENTER] > PROP_MIN)
//        pkt_to_tx[4] |= 0x02;
//    pkt_to_tx[4] |= headArrayBspDigitalState(HEAD_ARRAY_SENSOR_RIGHT) ? 0x04 : 0x00;
//    if (g_PadProportionalValue[HEAD_ARRAY_SENSOR_RIGHT] > PROP_MIN)
//        pkt_to_tx[4] |= 0x08;
//    pkt_to_tx[4] |= headArrayBspDigitalState(HEAD_ARRAY_SENSOR_LEFT) ? 0x10 : 0x00;
//    if (g_PadProportionalValue[HEAD_ARRAY_SENSOR_LEFT] > PROP_MIN)
//        pkt_to_tx[4] |= 0x20;
}

//-------------------------------
// Function: BuildAckPacket
//
// Description: Builds up a packet to send "ACK" as a response.
//
//-------------------------------
static void BuildAckPacket(uint8_t *pkt_to_tx)
{
    pkt_to_tx[0] = 3;
    pkt_to_tx[1] = HA_HHP_RESP_ACK;
}

//-------------------------------
// Function: BuildNackPacket
//
// Description: Builds up a packet to send "NACK" as a response.
//
//-------------------------------
static void BuildNackPacket(uint8_t *pkt_to_tx)
{
    pkt_to_tx[0] = 3;
    pkt_to_tx[1] = HA_HHP_RESP_NACK;
}

//-------------------------------
// Function: CalcChecksum
//
// Description: Calculates the checksum for a packet.
//
//-------------------------------
static uint8_t CalcChecksum(uint8_t *packet, uint8_t len)
{
    uint8_t checksum = 0;

    for (unsigned int i = 0; i < len; i++)
    {
        checksum += packet[i];
    }

    return checksum;
}

//-------------------------------
// Function: BuildEnabledFeaturesResponsePacket
//
// Description: This function crates the response to a GET FEATURES command
//-------------------------------

static void BuildEnabledFeaturesResponsePacket(uint8_t *pkt_to_tx)
{
    pkt_to_tx[0] = 5;
    pkt_to_tx[1] = 0x3f; // eeprom8bitGet(EEPROM_STORED_ITEM_ENABLED_FEATURES);
    pkt_to_tx[2] = 14; // (uint8_t)(eeprom16bitGet(EEPROM_STORED_ITEM_USER_BTN_LONG_PRESS_ACT_TIME) / 100);
    pkt_to_tx[3] = 0; // eeprom8bitGet(EEPROM_STORED_ITEM_ENABLED_FEATURES_2);
}



