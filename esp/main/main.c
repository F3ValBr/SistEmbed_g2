#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "driver/i2c.h"
#include "driver/uart.h"
#include "embebidos/FFT.h"
#include "embebidos/bme.h"
#include "embebidos/nvs_embebidos.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"
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

// Función para extraer los 5 mayores números por presion, temperatura, humedad
float **extraer_top_5(bme_data reads[], size_t n) {
    // Reservar memoria para el arreglo que contendrá los 5 mayores números
    float **top5 = (float **)malloc(4 * sizeof(float *));
    if (top5 == NULL) {
        // printf("Error al asignar memoria\n");
        return NULL;
    }

    top5[0] = (float *)malloc(5 * sizeof(float));  // Temperatura
    top5[1] = (float *)malloc(5 * sizeof(float));  // Presion
    top5[2] = (float *)malloc(5 * sizeof(float));  // Humedad
    top5[3] = (float *)malloc(5 * sizeof(float));  // Gas

    if (top5[0] == NULL || top5[1] == NULL || top5[2] == NULL) {
        // printf("Error al asignar memoria\n");
        free(top5[0]);
        free(top5[1]);
        free(top5[2]);
        free(top5[3]);
        free(top5);
        return NULL;
    }

    // Inicializar los arreglos
    for (int i = 0; i < 5; i++) {
        top5[0][i] = -FLT_MAX;
        top5[1][i] = -FLT_MAX;
        top5[2][i] = -FLT_MAX;
        top5[3][i] = -FLT_MAX;
    }

    // Extraer los 5 mayores números
    for (size_t i = 0; i < n; i++) {
        float curr_pres = (float)reads[i].presure;
        float curr_temp = (float)reads[i].temperature;
        float curr_hum = (float)reads[i].humidity;
        float curr_gas = (float)reads[i].gas_resistance;

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

        for (int j = 0; j < 5; j++) {
            if (curr_hum > top5[2][j]) {
                for (int k = 4; k > j; k--) {
                    top5[2][k] = top5[2][k - 1];
                }
                top5[2][j] = curr_hum;
                break;
            }
        }

        for (int j = 0; j < 5; j++) {
            if (curr_gas > top5[3][j]) {
                for (int k = 4; k > j; k--) {
                    top5[3][k] = top5[3][k - 1];
                }
                top5[3][j] = curr_gas;
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

void calcular_rms(bme_data readings[], size_t n, float *rms_temp, float *rms_pres, float *rms_hum, float *rms_gas) {
    // Crear arreglos temporales para las temperaturas y presiones
    int *temperaturas = (int *)malloc(n * sizeof(int));
    int *presiones = (int *)malloc(n * sizeof(int));
    int *humedades = (int *)malloc(n * sizeof(int));
    int *gases = (int *)malloc(n * sizeof(int));

    if (temperaturas == NULL || presiones == NULL || humedades == NULL) {
        // printf("Error al asignar memoria para los cálculos RMS\n");
        free(temperaturas);
        free(presiones);
        free(humedades);
        free(gases);
        return;
    }

    // Llenar los arreglos con los datos de temperatura y presión
    for (size_t i = 0; i < n; i++) {
        temperaturas[i] = readings[i].temperature;
        presiones[i] = readings[i].presure;
        humedades[i] = readings[i].humidity;
        gases[i] = readings[i].gas_resistance;
    }

    // Calcular el RMS para temperatura y presión
    *rms_temp = rmsValue(temperaturas, n);
    *rms_pres = rmsValue(presiones, n);
    *rms_hum = rmsValue(humedades, n);
    *rms_gas = rmsValue(gases, n);

    // Liberar memoria de los arreglos temporales
    free(temperaturas);
    free(presiones);
    free(humedades);
    free(gases);
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

void bme_data_sender(bme_data *data, float **top5, float rms_temp, float rms_pres, float rms_hum, float rms_gas, int32_t window) {
    // Inicializar la comunicación
    char dataResponse1[6];
    // printf("Beginning initialization... \n");
    while (1) {
        int rLen = serial_read(dataResponse1, 6);
        if (rLen > 0) {
            if (strcmp(dataResponse1, "BEGIN") == 0) {
                // uart_write_bytes(UART_NUM,"OK\0",3);
                // printf("Initialization complete\n");
                break;
            }
        }
    }
    // printf("Begin sending... \n");
    //  Enviar los datos
    float data_point[4];
    for (int i = 0; i < window; i++) {
        data_point[0] = data[i].temperature;
        data_point[1] = data[i].presure;
        data_point[2] = data[i].humidity;
        data_point[3] = data[i].gas_resistance;

        // printf("Temperatura: %f\n", data_point[0]);
        // printf("Presion: %f\n", data_point[1]);
        // printf("Humedad: %f\n", data_point[2]);
        // printf("Gas: %f\n", data_point[3]);

        uart_write_bytes(UART_NUM, (const char *)data_point, sizeof(float) * 4);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Enviar los top 5 y el RMS
    float data_top5[20];
    for (int i = 0; i < 5; i++) {
        data_top5[i] = top5[0][i];
        data_top5[i + 5] = top5[1][i];
        data_top5[i + 10] = top5[2][i];
        data_top5[i + 15] = top5[3][i];
    }
    // Enviar de a 5 valores el top 5
    for (int i = 0; i < 4; i++) {
        uart_write_bytes(UART_NUM, (const char *)&data_top5[i * 5], sizeof(float) * 5);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    float data_rms[4];
    data_rms[0] = rms_temp;
    data_rms[1] = rms_pres;
    data_rms[2] = rms_hum;
    data_rms[3] = rms_gas;
    uart_write_bytes(UART_NUM, (const char *)data_rms, sizeof(float) * 4);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

int HAS_SENT_WINDOW_YET = 0;

/**
 * @brief Separa valores leídos según dato.
 *
 * @param temp Arreglo de temperaturas a llenar.
 * @param hum Arreglo de humedades a llenar.
 * @param gas Arreglo de gas a llenar.
 * @param pres Arreglo de presión a llenar.
 * @param data Arreglo de origen con datos agrupados.
 * @param window_size Tamaño de arreglo.
 */
void fill_THCP_arrays(float *temp, float *hum, float *gas, float *pres, bme_data *data, int window_size) {
    for (int i = 0; i < window_size; i++) {
        temp[i] = data[i].temperature;
        hum[i] = data[i].humidity;
        gas[i] = data[i].gas_resistance;
        pres[i] = data[i].presure;
    }
}

// Función para manejar los comandos recibidos por UART
void command_handler(uint8_t signal_type, uint32_t body) {
    switch (signal_type) {
        case 0:
            int32_t window = read_window_nvs();
            // Enviar la ventana
            unsigned long long window_float = (unsigned long long)window;
            uart_write_bytes(UART_NUM, (const char *)&window_float, sizeof(unsigned long long));
            vTaskDelay(pdMS_TO_TICKS(1000));
            
            // printf("Ventana actual: %ld\n", window);
            //  Enviar ventana y calcular datos
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

            // Calcular el RMS
            float rms_temp = 0.0, rms_pres = 0.0, rms_hum = 0.0, rms_gas = 0.0;
            calcular_rms(data, n_reads, &rms_temp, &rms_pres, &rms_hum, &rms_gas);
            // printf("RMS Temperatura: %f\n", rms_temp);
            // printf("RMS Presion: %f\n", rms_pres);
            // printf("RMS Humedad: %f\n", rms_hum);

            // Calcular FFT

            float *array_re = malloc(sizeof(float) * window);
            float *array_im = malloc(sizeof(float) * window);
            float *temp = malloc(sizeof(float) * window);
            float *hum = malloc(sizeof(float) * window);
            float *gas = malloc(sizeof(float) * window);
            float *pres = malloc(sizeof(float) * window);
            fill_THCP_arrays(temp, hum, gas, pres, data, window);
            calcularFFT(temp, window, array_re, array_im);
            calcularFFT(hum, window, array_re, array_im);
            calcularFFT(gas, window, array_re, array_im);
            calcularFFT(pres, window, array_re, array_im);

            // Enviar los datos al controlador
            bme_data_sender(data, top5, rms_temp, rms_pres, rms_hum, rms_gas, window);

            // Liberar memoria
            free(data);
            free(top5[0]);
            free(top5[1]);
            free(top5[2]);
            free(top5[3]);
            free(top5);
            free(array_re);
            free(array_im);
            free(temp);
            free(hum);
            free(gas);
            free(pres);
            break;
        case 1:
            write_window_nvs(body);
            // printf("Ventana cambiada a: %ld\n", body);
            break;
        case 2:
            // printf("Cerrando comunicación\n");
            esp_restart();
            break;
        default:
            // printf("Comando no reconocido\n");
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

    char signal_buffer[5];

    while (true) {
        // Espera señal
        if (serial_read(signal_buffer, 5) == 0) {
            // printf("esperando señal");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }
        // Reacciona al tipo de señal
        uint8_t signal_type = signal_buffer[0];

        // uint32_t *signal_body = &signal_buffer[1];
        //  Convierte los siguientes 4 bytes a uint32_t
        uint32_t signal_body;
        memcpy(&signal_body, &signal_buffer[1], sizeof(uint32_t));

        // command_handler(signal_type, *signal_body);
        command_handler(signal_type, signal_body);
        vTaskDelay(pdMS_TO_TICKS(1000));
        // fflush(stdout);
    }
}
