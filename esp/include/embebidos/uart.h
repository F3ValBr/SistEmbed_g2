#pragma once
#include "driver/uart.h"
#include "embebidos/THCP_monitor.h"

#define BUF_SIZE (128)       // buffer size
#define TXD_PIN 1            // UART TX pin
#define RXD_PIN 3            // UART RX pin
#define UART_NUM UART_NUM_0  // UART port number
#define BAUD_RATE 115200     // Baud rate
#define WRITE_DELAY_MS 1000  // Tiempo de espera post escritura

int uart1_printf(const char *str, va_list ap);
void uart_setup();
int serial_read(char *buffer, int size);
int serial_write_0(const char *msg, int len);

typedef enum msg_ack {
    OK,       // No hay problema con el mensaje.
    CORRUPT,  // Problema con largo de mensaje o checksum.
    INVALID   // Paquete íntegro pero con datos no válidos.
} msg_ack;

/**
 * @brief Envía un mensaje esperando por confirmación
 *
 * @param msg Puntero al mensaje.
 * @param msg_size Tamaño del mensaje.
 * @return msg_ack ACK recibido.
 */
msg_ack send_n_confirm(char *buffer, int msg_size);