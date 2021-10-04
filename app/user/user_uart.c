#include <ctype.h>
#include "ets_sys.h"
#include "uart.h"
#include "osapi.h"
#include "user_interface.h"
#include "user_config.h"
#include "zlib.h"
#include "user_uart.h"
#include "user_json.h"

static uint8_t send_mac[40];
static uint8_t rec_dat[40];
static uint8_t rec_length = 0;
static uint8_t xor_check = 0;
void ICACHE_FLASH_ATTR user_uart_receive(uint8_t dat)
{
    uint8_t i;
    //uart_tx_one_char(UART0, dat);
    //return ;
    switch (rec_length)
    {
        case 0:
        {
            if(dat == 0xaa || dat == 'A')
            {
                rec_dat[0] = dat;
                rec_length++;
                xor_check = 0;
            }
            break;
        }
        default:
        {
            if(rec_dat[0] == 'A' && (dat == '\r' || dat == '\n'))
            {
                rec_dat[rec_length] = '\0';

                if(os_strcmp(rec_dat, "AT+RESTORE") == 0)
                {
                    uart0_sendStr("\r\nOK\r\n");
                    zlib_wifi_AP();

                }
                else if(os_strcmp(rec_dat, "ATE0") == 0)
                {
                    uart0_sendStr("\r\nOK\r\n");
                }
                else if(os_strncmp(rec_dat, "AT+CIPSTAMAC", 12) == 0)
                {
                    uart0_sendStr(send_mac);
                }
                else /*if( os_strcmp(rec_dat, "AT+CWMODE=1") == 0
                 || os_strcmp(rec_dat, "ATE0") == 0
                 || os_strcmp(rec_dat, "AT+CWSTARTSMART=1") == 0)*/
                {
                    uart0_sendStr("\r\nOK\r\n");
                    uart0_sendStr(rec_dat);
                }
                /*else
                 {
                 uart0_sendStr(rec_dat);
                 }*/
                rec_length = 0;
                break;
            }

            rec_dat[rec_length] = dat;
            rec_length++;
            xor_check ^= dat;

            if(rec_dat[0] == 0xaa && rec_length > 1 && rec_length == rec_dat[1] + 2)
            {
                if(xor_check != 0x00)
                {
                    os_printf("xor_check error!");
                    os_printf("REC:");
                    for (i = 0; i < rec_length; i++)
                        os_printf("%x ", rec_dat[i]);
                    os_printf("\n");

                    rec_length = 0;
                    break;
                }

                user_sensor.cmd = rec_dat[2];
                user_sensor.msgID1 = ((uint16_t) rec_dat[9] << 8) | rec_dat[10];
                user_sensor.reed = rec_dat[11];
                user_sensor.now.year = BCDtoDEC(rec_dat[12]) + 2000;
                user_sensor.now.month = BCDtoDEC(rec_dat[13]);
                user_sensor.now.day = BCDtoDEC(rec_dat[14]);
                user_sensor.now.hour = BCDtoDEC(rec_dat[15]);
                user_sensor.now.minute = BCDtoDEC(rec_dat[16]);
                user_sensor.now.second = BCDtoDEC(rec_dat[17]);

                user_sensor.msgID2 = rec_dat[18];
                user_sensor.battery = rec_dat[19];
                user_sensor.alert = rec_dat[20];

                user_sensor.alert_time_start.hour = rec_dat[21];
                user_sensor.alert_time_start.minute = rec_dat[22];
                user_sensor.alert_time_end.hour = rec_dat[23];
                user_sensor.alert_time_end.minute = rec_dat[24];

                if(zlib_mqtt_is_connected() == true)
                {
                    char str[40];
                    os_sprintf(str, "{\"mac\":\"%s\",\"sensor\":%d,\"battery\":%d}", zlib_wifi_get_mac_str(),
                            user_sensor.reed - 1, user_sensor.battery);
                    zlib_fun_wifi_send(NULL, WIFI_COMM_TYPE_MQTT, user_mqtt_get_state_topic(), str, 1, 1);
                    if(user_sensor.cmd == 0x81) user_sensor.cmd = 0x91;

                    uint64_t current_stamp_temp = sntp_get_current_timestamp();

                    if(current_stamp_temp > 0)
                    {
                        zlib_rtc_get_time(current_stamp_temp, &user_sensor.now);
                    }
                    user_sensor.alert = 0xff;
                    user_uart_send_sensor(&user_sensor);
                }

                rec_length = 0;
            }

            break;
        }
    }
}

void ICACHE_FLASH_ATTR user_uart_send_sensor(user_sensor_t *sensor)
{
    uint8_t i;
    uint8_t dat[32];
    dat[0] = 0xaa;
    dat[1] = 0x10;

    dat[2] = sensor->cmd;

    dat[3] = (sensor->msgID1 >> 8) & 0xff;
    dat[4] = (sensor->msgID1) & 0xff;

    dat[5] = DECtoBCD(sensor->now.year - 2000);
    dat[6] = DECtoBCD(sensor->now.month);
    dat[7] = DECtoBCD(sensor->now.day);
    dat[8] = DECtoBCD(sensor->now.hour);
    dat[9] = DECtoBCD(sensor->now.minute);
    dat[10] = DECtoBCD(sensor->now.second);
    dat[11] = sensor->msgID2;

    dat[12] = sensor->alert;

    dat[13] = sensor->alert_time_start.hour;
    dat[14] = sensor->alert_time_start.minute;
    dat[15] = sensor->alert_time_end.hour;
    dat[16] = sensor->alert_time_end.minute;

    dat[17] = 0;
    for (i = 0; i < 17; i++)
        dat[17] ^= dat[i];

    for (i = 0; i < 18; i++)
        uart_tx_one_char(UART0, dat[i]);
}

void ICACHE_FLASH_ATTR user_uart_init()
{
    uint8_t *p = zlib_wifi_get_mac();
    os_sprintf(send_mac, "+CIPSTAMAC:\""MACSTR"\"\r\n\r\nOK\r\n", MAC2STR(p));
    os_printf("%s", send_mac);

}
