#ifndef __USER_UART_H__
#define __USER_UART_H__

extern void user_uart_receive(uint8_t dat);
extern void user_uart_send_sensor(user_sensor_t *sensor);

extern void user_uart_init();
#endif

