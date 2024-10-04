#pragma once
#include "driver/uart.h"
#include "embebidos/THCP_monitor.h"

#define BUF_SIZE (128)       // buffer size
#define TXD_PIN 1            // UART TX pin
#define RXD_PIN 3            // UART RX pin
#define UART_NUM UART_NUM_0  // UART port number
#define BAUD_RATE 115200     // Baud rate

int uart1_printf(const char *str, va_list ap);
void uart_setup();
int serial_read(char *buffer, int size);
int serial_write_0(const char *msg, int len);