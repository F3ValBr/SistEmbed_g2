#include "embebidos/uart.h"

#include <string.h>

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