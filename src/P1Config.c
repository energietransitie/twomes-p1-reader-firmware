#include "P1Config.h"
#include "abr.h"

#define P1_MEASUREMENT_INTERVAL_MS 5 * 60 * 1000 //milliseconds (5 min * 60  s/min * 1000 ms/s)

/**
 * @brief Initialise UART "P1PORT_UART_NUM" for P1 receive
 */
void initP1UART_DSMR45() {
    //UART Configuration for P1-Port reading:
    
    //115200 baud, 8n1, no HW flow control
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    ESP_ERROR_CHECK(uart_param_config(P1PORT_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(P1PORT_UART_NUM, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));// Set UART pins(TX: IO17, RX: IO16, RTS: *, CTS: *)
    ESP_ERROR_CHECK(uart_set_line_inverse(P1PORT_UART_NUM, UART_SIGNAL_RXD_INV | UART_SIGNAL_IRDA_RX_INV)); //Invert RX data
    ESP_ERROR_CHECK(uart_driver_install(P1PORT_UART_NUM, P1_BUFFER_SIZE * 2, 0, 0, NULL, 0));
}

/**
 * @brief Initialise UART "P1PORT_UART_NUM" for P1 receive
 */
void initP1UART_DSMR23() {
    //UART Configuration for P1-Port reading:
    
    //9600 baud, 7E1, no HW flow control
    uart_config_t uart_config = {
        .baud_rate = 9600,
        .data_bits = UART_DATA_7_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .rx_flow_ctrl_thresh = 122,
    };

    ESP_ERROR_CHECK(uart_param_config(P1PORT_UART_NUM, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(P1PORT_UART_NUM, 17, 16, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));// Set UART pins(TX: IO17, RX: IO16, RTS: *, CTS: *)
    ESP_ERROR_CHECK(uart_set_line_inverse(P1PORT_UART_NUM, UART_SIGNAL_RXD_INV | UART_SIGNAL_IRDA_RX_INV)); //Invert RX data
    ESP_ERROR_CHECK(uart_driver_install(P1PORT_UART_NUM, P1_BUFFER_SIZE * 2, 0, 0, NULL, 0));
}

/**
 * @brief Initalise pushbuttons, LEDs and Data-Request pin
 */
void initGPIO_P1() {
    gpio_config_t io_conf;
    //CONFIGURE OUTPUTS:
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = OUTPUT_BITMASK;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    //CONFIGURE INPUTS:
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = INPUT_BITMASK;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
}

/**
 * @brief calculate the CRC16 of the P1 message (A001 polynomial)
 *
 * @param crc starting value of the crc, to allow for cumulative CRC (init with 0x0000 when scanning full message at once)
 * @param buf pointer to the string to calculate the crc on
 * @param len the length of the string
 *
 * @return the calculated CRC16
 */
unsigned int CRC16(unsigned int crc, unsigned char *buf, int len) {
    for (int pos = 0; pos < len; pos++) {
        crc ^= (unsigned int)buf[pos]; // XOR byte into least sig. byte of crc

        for (int i = 8; i != 0; i--) { // Loop over each bit
            if ((crc & 0x0001) != 0) {// If the LSB is set
                crc >>= 1; // Shift right and XOR 0xA001
                crc ^= 0xA001;
            }
            else           // Else LSB is not set
                crc >>= 1; // Just shift right
        }
    }
    return crc;
}

/**
 * @brief Convert data read from P1 port into the struct
 *
 * @param p1String String received from the P1 port
 * @param p1Struct pointer to struct to save the data in
 *
 * @return Success (0) or error for missing datatype for error-check
 *
 */
int p1StringToStruct(const char *p1String, P1Data *p1Struct) {
    //Use strstr() from string library to find OBIS references to datatypes of P1 message
    //See https://www.netbeheernederland.nl/_upload/Files/Slimme_meter_15_a727fce1f1.pdf at page 20 for definitions:

    //strstr() returns null when string is not found, can be used to check for errors

    //REVIEW: should packaging into struct be omitted and directly package into JSON format?ol

    //DSMR version: OBIS reference 1-3:0.2.8
    char *dsmrPos = strstr(p1String, "1-3:0.2.8");
    if (dsmrPos != NULL) {
        //Read the version number:
        sscanf(dsmrPos, "1-3:0.2.8(%hhu", &(p1Struct->dsmrVersion)); //Read DSMR version as unsigned char
    }
    //else
      //  return P1_ERROR_DSMR_NOT_FOUND; //DSMR version not found is not an error

    //elecUsedT1 OBIS reference: 1-0:1.8.1
    char *elecUsedT1Pos = strstr(p1String, "1-0:1.8.1");
    if (elecUsedT1Pos != NULL) {
        //read the ElecUsedT1, specification states fixed 3 decimal float:
        sscanf(elecUsedT1Pos, "1-0:1.8.1(%lf", &(p1Struct->elecUsedT1));
    }
    else
        return P1_ERROR_ELECUSEDT1_NOT_FOUND;

    //elecUsedT2 OBIS reference: 1-0:1.8.2
    char *elecUsedT2Pos = strstr(p1String, "1-0:1.8.2");
    if (elecUsedT2Pos != NULL) {
        sscanf(elecUsedT2Pos, "1-0:1.8.2(%lf", &p1Struct->elecUsedT2);
    }
    else
        return P1_ERROR_ELECUSEDT2_NOT_FOUND;

    //elecReturnT1 OBIS reference: 1-0:2.8.1
    char *elecReturnT1Pos = strstr(p1String, "1-0:2.8.1");
    if (elecReturnT1Pos != NULL) {
        sscanf(elecReturnT1Pos, "1-0:2.8.1(%lf", &p1Struct->elecDeliveredT1);
    }
    else
        return P1_ERROR_ELECRETURNT2_NOT_FOUND;


    //elecReturnT2 OBIS reference 1-0:2.8.2
    char *elecReturnT2Pos = strstr(p1String, "1-0:2.8.2");
    if (elecReturnT2Pos != NULL) {
        sscanf(elecReturnT2Pos, "1-0:2.8.2(%lf", &p1Struct->elecDeliveredT2);
    }
    else
        return P1_ERROR_ELECRETURNT2_NOT_FOUND;

uint32_t currentBaud = 0;
uart_get_baudrate(P1PORT_UART_NUM, &currentBaud);
if(currentBaud < 115200)//dsmr2/3 smart meters
{
    //elec Timestamp OBIS reference 
    char *elecTimePos = strstr(p1String, "0-0:24.3.0");
    if (elecTimePos != NULL) {
        sscanf(elecTimePos, "0-0:24.3.0(%12s", p1Struct->timeElecMeasurement);
        p1Struct->timeElecMeasurement[12] = 0; //add a zero terminator at the end to read as string
    }
}
else
{
    //elec Timestamp OBIS reference 
    char *elecTimePos = strstr(p1String, "0-0:1.0.0");
    if (elecTimePos != NULL) {
        sscanf(elecTimePos, "0-0:1.0.0(%13s", p1Struct->timeElecMeasurement);
        p1Struct->timeElecMeasurement[13] = 0; //add a zero terminator at the end to read as string
    }
}

#ifdef DSMR2OR3
    //DSMR 2.2 had different layout of gas timestap and gas reading
    //Gas reading OBIS: 0-n:24.3.0 //n can vary depending on which channel it is installed
    char *gasTimePos = strstr(p1String, "0-1:24.3.0");
    if (gasTimePos != NULL) {
        sscanf(gasTimePos, "0-1:24.3.0(%12s)", p1Struct->timeGasMeasurement);
        p1Struct->timeGasMeasurement[12] = 0; //Add a null terminator to print it as a string
    }
    else
        return P1_ERROR_GAS_READING_NOT_FOUND;
    char *gasPos = strstr(p1String, "m3)");
    if (gasTimePos != NULL) {
        sscanf(gasPos, "m3)\n(%lf)", &p1Struct->gasUsage);
    }
    else
        return P1_ERROR_GAS_READING_NOT_FOUND;
#else
    //Gas reading OBIS: 0-n:24.2.1 //n can vary depending on which channel it is installed
    char *gasPos = strstr(p1String, "0-1:24.2.1");
    if (gasPos != NULL) {
        sscanf(gasPos, "0-1:24.2.1(%13s)(%lf)", p1Struct->timeGasMeasurement, &p1Struct->gasUsage);
        p1Struct->timeGasMeasurement[13] = 0; //Add a null terminator to print it as a string
    }
    else
        return P1_ERROR_GAS_READING_NOT_FOUND;
#endif
    //If none of the statements reached an "else" all measurements were read correctly!
    return P1_READ_OK;
}


/**
 * @brief print P1 struct data to UART1/LOGI
 *
 * @param data pointer to P1Data type struct
 *
 */
void printP1Data(P1Data *data) {
    ESP_LOGI("P1 Print", "DSMR VERSION %i ", data->dsmrVersion);
    ESP_LOGI("P1 Print", "e_use_lo_cum__kWh: %4.3f ", data->elecUsedT1);
    ESP_LOGI("P1 Print", "e_use_hi_cum__kWh: %4.3f ", data->elecUsedT2);
    ESP_LOGI("P1 Print", "e_ret_lo_cum__kWh: %4.3f ", data->elecDeliveredT1);
    ESP_LOGI("P1 Print", "e_ret_hi_cum__kWh: %4.3f ", data->elecDeliveredT2);
    ESP_LOGI("P1 Print", "ELEC TIMESTAMP: %s", data->timeElecMeasurement);
    ESP_LOGI("P1 Print", "g_use_cum__m3:  %7.3f ", data->gasUsage);
    ESP_LOGI("P1 Print", "GAS TIMESTAMP: %s ", data->timeGasMeasurement);
}

/**
 * @brief print P1 Error type to serial monitor with explanation
 *
 * @param errorType the errortype returned from parsing P1 data
 *
 */
void printP1Error(int errorType) {
    switch (errorType) {
        case P1_ERROR_DSMR_NOT_FOUND:
            ESP_LOGI("P1 ERROR", "DSMR version could not be found");
            break;
        case P1_ERROR_ELECUSEDT1_NOT_FOUND:
            ESP_LOGI("P1_ERROR", "Electricity used Tariff 1 not found");
            break;
        case P1_ERROR_ELECUSEDT2_NOT_FOUND:
            ESP_LOGI("P1_ERROR", "Electricity used Tariff 2 not found");
            break;
        case P1_ERROR_ELECRETURNT1_NOT_FOUND:
            ESP_LOGI("P1_ERROR", "Electricity returned Tariff 1 not found");
            break;
        case P1_ERROR_ELECRETURNT2_NOT_FOUND:
            ESP_LOGI("P1_ERROR", "Electricity returned Tariff 2 not found");
            break;
        case P1_ERROR_GAS_READING_NOT_FOUND:
            ESP_LOGI("P1_ERROR", "Gas reading not found");
            break;
        case P1_ERROR_ELEC_TIMESTAMP_NOT_FOUND:
            ESP_LOGI("P1_ERROR", "Electricity timestamp not found");
            break;
        default:
            break;
    }
}

P1Data p1Read()
{
    //little init check to prevent unnecessery loops.
    static int uartInit = 0;
    if (!uartInit)
    {
        initP1UART_DSMR45();
	    initGPIO_P1();
        //set baudrate
        uartStartDetectBaudrate();
        uartInit = 1;
    }

    //create struct
    P1Data p1Measurements;
    //Empty the buffer before requesting data to clear it of junk
    uart_flush(P1PORT_UART_NUM);
    ESP_LOGI("P1", "Attempting to read P1 Port");
    //DRQ pin has inverter to pull up to 5V, which makes it active low:      
    gpio_set_level(PIN_DRQ, 0);
    //Wait for 18 seconds to ensure a message is read even on a DSMR4.x device:
    vTaskDelay(18000 / portTICK_PERIOD_MS);
    //Write DRQ pin low again (otherwise P1 port keeps transmitting every second);
    gpio_set_level(PIN_DRQ, 1);

    //Allcoate a buffer with the size of the P1 UART buffer to store P1 data:
    uint8_t *data = (uint8_t *)malloc(P1_BUFFER_SIZE);

    //Read data from the P1 UART:
    int uartDataSize = uart_read_bytes(P1PORT_UART_NUM, data, P1_BUFFER_SIZE, 20 / portTICK_PERIOD_MS);

    //If data is received:
    if (uartDataSize > 0) {
        ESP_LOGD("P1", "data: %.*s", uartDataSize, data);
        //Trim the received message to contain only the necessary data and store the CRC as an unsigned int:
        char *p1MessageStart = strchr((const char *)data, '/'); //Find the position of the start-of-message character ('/')
        char *p1MessageEnd = NULL;
        //Only look for end if a start is found:
        if (p1MessageStart != NULL) p1MessageEnd = strchr((const char *)p1MessageStart, '!');   //Find the position of the end-of-message character ('!')

        //Check if a message is received:
        if (p1MessageEnd != NULL) {

            //Convert the CRC from string to int:
            unsigned int receivedCRC;
            //Start the scanf one char after the end-of-message symbol (location of CRC16), and read a 4-symbol hex number
            sscanf(p1MessageEnd + 1, "%4X", &receivedCRC);
            //Allocate memory to copy the trimmed message into
            uint8_t *p1Message = malloc(P1_MESSAGE_SIZE);
            //Trim the message to only include 1 full P1 port message:
            uartDataSize = (int) (p1MessageEnd - p1MessageStart) + 1;
            memcpy(p1Message, p1MessageStart, uartDataSize);
            p1Message[uartDataSize] = 0; //Add zero terminator to end of message
            ESP_LOGD("P1", "Trimmed message length: %d bytes)", uartDataSize);

            //Calculate the CRC of the trimmed message:
            unsigned int calculatedCRC = CRC16(0x0000, p1Message, uartDataSize);
            #ifdef DSMR2OR3
                receivedCRC = calculatedCRC;
            #endif
            //Check if CRC match:
            if (calculatedCRC == receivedCRC) {
                //log received CRC and calculated CRC for debugging
                ESP_LOGD("P1", "Received matching CRC: (%4X == %4X)", receivedCRC, calculatedCRC);
                ESP_LOGI("P1", "Parsing message into struct:");
            
                //extract the necessary data from the P1 payload into the struct and check for errors while decoding
                int result = p1StringToStruct((const char *)p1Message, &p1Measurements);

                if (result == P1_READ_OK) {
                    //Print the data from the struct to monitor for debugging:
                    printP1Data(&p1Measurements);                   

                    //for testing purposes the data is stored in a textfile
                    return p1Measurements;
                }
                else {
                    //If a measurement could not be read, print to serial terminal which one was (the first that was) missing
                    printP1Error(result);
                }
                //Start decoding the P1 message:
            }
            //if CRC does not match:
            else {
                //Log received and calculated CRC for debugging and flash the Error LED
                ESP_LOGE("ERROR - P1", "CRC DOES NOT MATCH");
                ESP_LOGD("ERROR - P1", "Received CRC %4X but calculated CRC %4X", receivedCRC, calculatedCRC);

            }

            //Free the P1 message from memory
            free(p1Message);
        }
        else {
            ESP_LOGE("P1", "P1 message was invalid");
        }
    } else if(uartDataSize == -1) {
        ESP_LOGI("P1", "No UART data found");

    }
    else{
        ESP_LOGI("P1", "No P1 message was found");
    }
    //Release the data from the memory buffer:
    free(data);

    return p1Measurements;

}
