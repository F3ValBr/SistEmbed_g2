#include "embebidos/uart.h"

#include <string.h>

#include "embebidos/checksum.h"
#include "esp_log.h"

// Function for sending things to UART1
int uart1_printf(const char *str, va_list ap) {
    char *buf;
    vasprintf(&buf, str, ap);
    uart_write_bytes(UART_NUM_1, buf, strlen(buf));
    free(buf);
    return 0;
}

// Setup of UART connections 0 and 1, and try to redirect logs to UART1 if asked
void uart_setup() {
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    };

    uart_param_config(UART_NUM_0, &uart_config);
    uart_param_config(UART_NUM_1, &uart_config);
    uart_driver_install(UART_NUM_0, BUF_SIZE * 2, 0, 0, NULL, 0);
    uart_driver_install(UART_NUM_1, BUF_SIZE * 2, 0, 0, NULL, 0);

    // Redirect ESP log to UART1
    if (REDIRECT_LOGS) {
        esp_log_set_vprintf(uart1_printf);
    }
}

// Read UART_num for input with timeout of 1 sec
int serial_read(char *buffer, int size) {
    int len = uart_read_bytes(UART_NUM, (uint8_t *)buffer, size, pdMS_TO_TICKS(1000));
    return len;
}

// Write message through UART_num with an \0 at the end
int serial_write_0(const char *msg, int len) {
    char *send_with_end = (char *)malloc(sizeof(char) * (len + 1));
    memcpy(send_with_end, msg, len);
    send_with_end[len] = '\0';

    int result = uart_write_bytes(UART_NUM, send_with_end, len + 1);

    free(send_with_end);

    vTaskDelay(pdMS_TO_TICKS(1000));  // Delay for 1 second
    return result;
}

/**
 * @brief Envía un mensaje por UART e inmediatamente invoca un delay según la
 * definición en header.
 *
 * @param data Datos a enviar.
 * @param size Tamaño de paquete a enviar.
 */
void uart_write_delay(void *data, size_t size) {
    uart_write_bytes(UART_NUM, (const char *)data, size);
    vTaskDelay(pdMS_TO_TICKS(WRITE_DELAY_MS));
}

void write_pkg_init(char *pkg_start) {
    memcpy(pkg_start, "ABCD", 4);
}

void write_pkg_end(char *pkg_end) {
    memcpy(pkg_end, "DCBA", 4);
}

int get_pkg_end_index(int pkg_size) {
    int end_index = pkg_size - 4;
    return end_index;
}

int get_pkg_len(int msg_size) {
    // INICIO + CHECKSUM + LARGO + msg_size + FIN
    int pkg_len = sizeof(char) * 4 + sizeof(unsigned int) +
                  sizeof(unsigned int) + msg_size + sizeof(char) * 4;
    return pkg_len;
}

/**
 * @brief Crea un paquete con INICIO de paquete, CHECKSUM de mensaje, LARGO de
 * mensaje, MENSAJE y FIN de paquete.
 *
 * @param pkg Puntero al paquete final.
 * @param buffer Puntero al mensaje a empaquetar.
 * @param size Tamaño del mensaje.
 */
void wrap_msg(char *pkg, char *buffer, int msg_size) {
    char *pkg_index = pkg;  // Cursor en posición a escribir.

    // Escribe inicio
    write_pkg_init(pkg);
    pkg_index += 4;

    // Escribe checksum
    unsigned int chksm = checksum(buffer);
    memcpy(pkg_index, &chksm, sizeof(unsigned int));
    pkg_index += sizeof(unsigned int);

    // Escribe largo de mensaje
    memcpy(pkg_index, &msg_size, sizeof(unsigned int));
    pkg_index += sizeof(unsigned int);

    // Escribe mensaje
    memcpy(pkg_index, buffer, msg_size);
    pkg_index += msg_size;

    // Escribe final
    write_pkg_end(pkg_index);
}

/**
 * @brief Envoltura para obtener tamaño del buffer con pausas en caso de haber
 * error.
 *
 * @param size Tamaño de buffer.
 */
void check_buf_len(size_t *size) {
    while (uart_get_buffered_data_len(UART_NUM, size) == ESP_FAIL) {
        vTaskDelay(pdMS_TO_TICKS(WRITE_DELAY_MS));
    }
}

/**
 * @brief Espera a haber un único byte en el buffer para extraer el ACK.
 *
 * @return msg_ack ACK extraído.
 */
msg_ack read_ack() {
    size_t buf_len;
    do {
        check_buf_len(&buf_len);
    } while (buf_len != 1);
    char ack;
    serial_read(&ack, sizeof(ack));
    return (msg_ack)ack;
}

msg_ack send_n_confirm(char *msg, int msg_size) {
    int pkg_len = get_pkg_len(msg_size);
    char pkg[pkg_len];
    wrap_msg(pkg, msg, msg_size);
    uart_write_delay(pkg, pkg_len);
    msg_ack ack = read_ack();
    while (ack == CORRUPT) {
        uart_write_delay(pkg, pkg_len);
        ack = read_ack();
    }
    return ack;
}