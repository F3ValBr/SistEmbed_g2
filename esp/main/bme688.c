#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "bme.h"
#include "driver/i2c.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"
#include "nvs_embebidos.h"
#include "sdkconfig.h"

#define CONCAT_BYTES(msb, lsb) (((uint16_t)msb << 8) | (uint16_t)lsb)

#define BUF_SIZE (128)       // buffer size
#define TXD_PIN 1            // UART TX pin
#define RXD_PIN 3            // UART RX pin
#define UART_NUM UART_NUM_0  // UART port number
#define BAUD_RATE 115200     // Baud rate
#define M_PI 3.14159265358979323846

#define I2C_MASTER_SCL_IO GPIO_NUM_22  // GPIO pin
#define I2C_MASTER_SDA_IO GPIO_NUM_21  // GPIO pin
#define I2C_MASTER_FREQ_HZ 10000
#define BME_ESP_SLAVE_ADDR 0x76
#define WRITE_BIT 0x0
#define READ_BIT 0x1
#define ACK_CHECK_EN 0x0
#define EXAMPLE_I2C_ACK_CHECK_DIS 0x0
#define ACK_VAL 0x0
#define NACK_VAL 0x1

#define REDIRECT_LOGS 1  // if redirect ESP log to another UART

// Función para extraer los 5 mayores números por presion y temperatura
float **extraer_top_5(bme_data reads[], size_t n) {
    // Reservar memoria para el arreglo que contendrá los 5 mayores números
    float **top5 = (float **)malloc(2 * sizeof(float *));
    if (top5 == NULL) {
        printf("Error al asignar memoria\n");
        return NULL;
    }

    top5[0] = (float *)malloc(5 * sizeof(float));  // Temperatura
    top5[1] = (float *)malloc(5 * sizeof(float));  // Presion

    if (top5[0] == NULL || top5[1] == NULL) {
        printf("Error al asignar memoria\n");
        free(top5[0]);
        free(top5[1]);
        free(top5);
        return NULL;
    }

    // Inicializar los arreglos
    for (int i = 0; i < 5; i++) {
        top5[0][i] = -FLT_MAX;
        top5[1][i] = -FLT_MAX;
    }

    // Extraer los 5 mayores números
    for (size_t i = 0; i < n; i++) {
        float curr_pres = (float)reads[i].presure;
        float curr_temp = (float)reads[i].temperature;

        for (int j = 0; j < 5; j++) {
            if (curr_temp > top5[0][j]) {
                for (int k = 4; k > j; k--) {
                    top5[0][k] = top5[0][k - 1];
                }
                top5[0][j] = curr_temp;
                break;
            }
        }

        for (int j = 0; j < 5; j++) {
            if (curr_pres > top5[1][j]) {
                for (int k = 4; k > j; k--) {
                    top5[1][k] = top5[1][k - 1];
                }
                top5[1][j] = curr_pres;
                break;
            }
        }
    }

    return top5;
}

// Function that Calculate Root Mean Square
float rmsValue(int arr[], int n) {
    int square = 0;
    float mean = 0.0, root = 0.0;

    // Calculate square.
    for (int i = 0; i < n; i++) {
        square += pow(arr[i], 2);
    }

    // Calculate Mean.
    mean = (square / (float)(n));

    // Calculate Root.
    root = sqrt(mean);

    return root;
}

void calcular_rms(bme_data readings[], size_t n, float *rms_temp, float *rms_pres) {
    // Crear arreglos temporales para las temperaturas y presiones
    int *temperaturas = (int *)malloc(n * sizeof(int));
    int *presiones = (int *)malloc(n * sizeof(int));

    if (temperaturas == NULL || presiones == NULL) {
        printf("Error al asignar memoria para los cálculos RMS\n");
        free(temperaturas);
        free(presiones);
        return;
    }

    // Llenar los arreglos con los datos de temperatura y presión
    for (size_t i = 0; i < n; i++) {
        temperaturas[i] = readings[i].temperature;
        presiones[i] = readings[i].presure;
    }

    // Calcular el RMS para temperatura y presión
    *rms_temp = rmsValue(temperaturas, n);
    *rms_pres = rmsValue(presiones, n);

    // Liberar memoria de los arreglos temporales
    free(temperaturas);
    free(presiones);
}

// Function for sending things to UART1
static int uart1_printf(const char *str, va_list ap) {
    char *buf;
    vasprintf(&buf, str, ap);
    uart_write_bytes(UART_NUM_1, buf, strlen(buf));
    free(buf);
    return 0;
}

// Setup of UART connections 0 and 1, and try to redirect logs to UART1 if asked
static void uart_setup() {
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

int HAS_SENT_WINDOW_YET = 0;

// CAMBIAR PARA QUE CONCUERDE CON PYTHON
void command_handler(uint8_t signal_type, uint32_t body) {
    switch (signal_type) {
        case 0:
            int32_t window = read_window_nvs();
            // printf("%li\n", window);
            // if (!HAS_SENT_WINDOW_YET) {
            //     HAS_SENT_WINDOW_YET = 1;
            //     serial_write_0((const char *)window, sizeof(int32_t));
            //     // uart_write_bytes(UART_NUM, (const char *)window, sizeof(int32_t));
            // }
            // Extraer la ventana
            printf("Ventana actual: %ld\n", window);
            // Enviar ventana y calcular datos
            size_t n_reads;
            bme_data *data = bme_read_data(window, &n_reads);
            if (data == NULL) {
                printf("Error al leer datos\n");
                break;
            }

            // Extraer los 5 mayores valores
            float **top5 = extraer_top_5(data, n_reads);
            if (top5 == NULL) {
                printf("Error al extraer los 5 mayores valores\n");
                free(data);
                break;
            }
            printf("Top 5 Temperaturas: ");
            for (int i = 0; i < 5; i++) {
                printf("%f ", top5[0][i]);
            }
            printf("\n");
            printf("Top 5 Presiones: ");
            for (int i = 0; i < 5; i++) {
                printf("%f ", top5[1][i]);
            }
            printf("\n");
            // Calcular el RMS
            float rms_temp = 0.0, rms_pres = 0.0;
            calcular_rms(data, n_reads, &rms_temp, &rms_pres);
            printf("RMS Temperatura: %f\n", rms_temp);
            printf("RMS Presion: %f\n", rms_pres);

            // Enviar los datos al controlador
            // AQUI EL PROCESO PARA MANDAR DATOS

            // tamano ventana
            // uart_write_bytes(UART_NUM, (const char *)window, sizeof(int32_t));
            // vTaskDelay(pdMS_TO_TICKS(50));

            // ventana
            serial_write_0((const char *)data, sizeof(bme_data) * window);
            // uart_write_bytes(UART_NUM, (const char *)data, sizeof(bme_data));
            // vTaskDelay(pdMS_TO_TICKS(50));

            // // top 5 y RMS
            // float pkg[12];
            // for (int i = 0; i < 5;) {
            //     pkg[i] = top5[0][i];
            //     pkg[i + 5] = top5[1][i];
            // }
            // pkg[10] = rms_temp;
            // pkg[11] = rms_pres;
            // // uart_write_bytes(UART_NUM, (const char *)pkg, sizeof(float) * 12);
            // // vTaskDelay(pdMS_TO_TICKS(50));
            // serial_write_0((const char *)pkg, sizeof(float) * 12);

            // Liberar memoria
            free(data);
            free(top5[0]);
            free(top5[1]);
            free(top5);
            break;
        case 1:
            write_window_nvs(body);
            printf("Ventana cambiada a: %ld\n", body);
            break;
        case 2:
            printf("Cerrando comunicación\n");
            esp_restart();
            break;
        default:
            printf("Comando no reconocido\n");
            break;
    }
}

void app_main(void) {
    uart_setup();    // Uart setup
    srand(time(0));  // Initialize random seed
    init_nvs();      // Inicializar NVS
    ESP_ERROR_CHECK(sensor_init());
    bme_get_chipid();
    bme_softreset();
    bme_get_mode();
    bme_forced_mode();
    // bme_softreset();  // Soft reset

    char signal_buffer[5];

    while (true) {
        // Espera señal
        if (serial_read(signal_buffer, 5) == 0) {
            // printf("esperando señal");
            continue;
        }
        // Reacciona al tipo de señal
        uint8_t signal_type = signal_buffer[0];

        uint32_t *signal_body = &signal_buffer[1];
        printf("antes de cmd handler");
        command_handler(signal_type, *signal_body);
        vTaskDelay(pdMS_TO_TICKS(1000));
        printf("fuera de cmd handler");
        // fflush(stdout);
    }
}
