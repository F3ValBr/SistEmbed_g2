#include <float.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// #include "driver/uart.h"
#include "embebidos/FFT.h"
// #include "embebidos/THCP_monitor.h"
// // #include "embebidos/bme.h"
#include "embebidos/bmi.h"
#include "embebidos/nvs_embebidos.h"
#include "embebidos/uart.h"
#include "esp_log.h"
#include "esp_system.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
#include "math.h"
#include "sdkconfig.h"

#define CONCAT_BYTES(msb, lsb) (((uint16_t)msb << 8) | (uint16_t)lsb)

// Función para extraer los 5 mayores números por presion, temperatura, humedad

float **bmi_extraer_top_5(bmi_data reads[], size_t n) {
    // Reservar memoria para el arreglo que contendrá los 5 mayores números
    float **top5 = (float **)malloc(6 * sizeof(float *));
    if (top5 == NULL) {
        printf("Error al asignar memoria\n");
        return NULL;
    }

    top5[0] = (float *)malloc(5 * sizeof(float));  // acc_x
    top5[1] = (float *)malloc(5 * sizeof(float));  // acc_y
    top5[2] = (float *)malloc(5 * sizeof(float));  // acc_z
    top5[3] = (float *)malloc(5 * sizeof(float));  // gyro_x
    top5[4] = (float *)malloc(5 * sizeof(float));  // gyro_y
    top5[5] = (float *)malloc(5 * sizeof(float));  // gyro_z

    if (
        top5[0] == NULL ||
        top5[1] == NULL ||
        top5[2] == NULL ||
        top5[3] == NULL ||
        top5[4] == NULL ||
        top5[5] == NULL) {
        printf("Error al asignar memoria\n");
        free(top5[0]);
        free(top5[1]);
        free(top5[2]);
        free(top5[3]);
        free(top5[4]);
        free(top5[5]);
        free(top5);
        return NULL;
    }

    // Inicializar los arreglos
    for (int i = 0; i < 5; i++) {
        top5[0][i] = -FLT_MAX;
        top5[1][i] = -FLT_MAX;
        top5[2][i] = -FLT_MAX;
        top5[3][i] = -FLT_MAX;
        top5[4][i] = -FLT_MAX;
        top5[5][i] = -FLT_MAX;
    }

    // Extraer los 5 mayores números
    for (size_t i = 0; i < n; i++) {
        float curr_acc_x = (float)reads[i].acc_x;
        float curr_acc_y = (float)reads[i].acc_y;
        float curr_acc_z = (float)reads[i].acc_z;
        float curr_gyro_x = (float)reads[i].gyro_x;
        float curr_gyro_y = (float)reads[i].gyro_y;
        float curr_gyro_z = (float)reads[i].gyro_z;

        for (int j = 0; j < 5; j++) {
            if (curr_acc_x > top5[0][j]) {
                for (int k = 4; k > j; k--) {
                    top5[0][k] = top5[0][k - 1];
                }
                top5[0][j] = curr_acc_x;
                break;
            }
        }

        for (int j = 0; j < 5; j++) {
            if (curr_acc_y > top5[1][j]) {
                for (int k = 4; k > j; k--) {
                    top5[1][k] = top5[1][k - 1];
                }
                top5[1][j] = curr_acc_y;
                break;
            }
        }

        for (int j = 0; j < 5; j++) {
            if (curr_acc_z > top5[2][j]) {
                for (int k = 4; k > j; k--) {
                    top5[2][k] = top5[2][k - 1];
                }
                top5[2][j] = curr_acc_z;
                break;
            }
        }

        for (int j = 0; j < 5; j++) {
            if (curr_gyro_x > top5[3][j]) {
                for (int k = 4; k > j; k--) {
                    top5[3][k] = top5[3][k - 1];
                }
                top5[3][j] = curr_gyro_x;
                break;
            }
        }

        for (int j = 0; j < 5; j++) {
            if (curr_gyro_y > top5[4][j]) {
                for (int k = 4; k > j; k--) {
                    top5[4][k] = top5[4][k - 1];
                }
                top5[4][j] = curr_gyro_y;
                break;
            }
        }

        for (int j = 0; j < 5; j++) {
            if (curr_gyro_z > top5[5][j]) {
                for (int k = 4; k > j; k--) {
                    top5[5][k] = top5[5][k - 1];
                }
                top5[5][j] = curr_gyro_z;
                break;
            }
        }
    }

    return top5;
}

// Function that Calculate Root Mean Square
float rmsValue(float arr[], float n) {
    float square = 0;
    float mean = 0.0, root = 0.0;

    // Calculate square.
    for (int i = 0; i < n; i++) {
        square += pow(arr[i], 2);
    }

    // Calculate Mean.
    mean = (square / n);

    // Calculate Root.
    root = sqrt(mean);

    return root;
}

void bmi_calcula_metricas(bmi_data readings[], size_t n,
                          float *rms_acc_x,
                          float *rms_acc_y,
                          float *rms_acc_z,
                          float *rms_gyro_x,
                          float *rms_gyro_y,
                          float *rms_gyro_z,
                          WindowFFT *acc_x_fft,
                          WindowFFT *acc_y_fft,
                          WindowFFT *acc_z_fft,
                          WindowFFT *gyro_x_fft,
                          WindowFFT *gyro_y_fft,
                          WindowFFT *gyro_z_fft) {
    // Crea arreglos temporales para desempaquetar gyro y accelerometro.
    float acc_x_samples[n];
    float acc_y_samples[n];
    float acc_z_samples[n];
    float gyro_x_samples[n];
    float gyro_y_samples[n];
    float gyro_z_samples[n];

    fill_bmi_arrays(
        acc_x_samples, acc_y_samples, acc_z_samples,
        gyro_x_samples, gyro_y_samples, gyro_z_samples,
        readings, n);

    // Calcula el RMS para ventana de cada coordenada registrada.
    *rms_acc_x = rmsValue(acc_x_samples, n);
    *rms_acc_y = rmsValue(acc_y_samples, n);
    *rms_acc_z = rmsValue(acc_z_samples, n);
    *rms_gyro_x = rmsValue(gyro_x_samples, n);
    *rms_gyro_y = rmsValue(gyro_y_samples, n);
    *rms_gyro_z = rmsValue(gyro_z_samples, n);

    // Calcula FFT para la ventana de cada coordenada registrada.
    calcularFFT(acc_x_samples, n, acc_x_fft->re_array, acc_x_fft->im_array);
    calcularFFT(acc_y_samples, n, acc_y_fft->re_array, acc_y_fft->im_array);
    calcularFFT(acc_z_samples, n, acc_z_fft->re_array, acc_z_fft->im_array);
    calcularFFT(gyro_x_samples, n, gyro_x_fft->re_array, gyro_x_fft->im_array);
    calcularFFT(gyro_y_samples, n, gyro_y_fft->re_array, gyro_y_fft->im_array);
    calcularFFT(gyro_z_samples, n, gyro_z_fft->re_array, gyro_z_fft->im_array);
}

void bmi_data_sender(
    bmi_data *data, float **top5,
    float rms_acc_x, float rms_acc_y, float rms_acc_z, float rms_gyro_x, float rms_gyro_y, float rms_gyro_z,
    WindowFFT *acc_x_fft, WindowFFT *acc_y_fft, WindowFFT *acc_z_fft, WindowFFT *gyro_x_fft, WindowFFT *gyro_y_fft, WindowFFT *gyro_z_fft,
    int32_t window) {
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
    float data_point[6];
    for (int i = 0; i < window; i++) {
        data_point[0] = data[i].acc_x;
        data_point[1] = data[i].acc_y;
        data_point[2] = data[i].acc_z;
        data_point[3] = data[i].gyro_x;
        data_point[4] = data[i].gyro_y;
        data_point[5] = data[i].gyro_z;

        // printf("Aceleración en x: %f\n", data_point[0]);
        // printf("Aceleración en y: %f\n", data_point[1]);
        // printf("Aceleración en z: %f\n", data_point[2]);
        // printf("Velocidad angular en x: %f\n", data_point[3]);
        // printf("Velocidad angular en y: %f\n", data_point[4]);
        // printf("Velocidad angular en z: %f\n", data_point[5]);

        uart_write_bytes(UART_NUM, (const char *)data_point, sizeof(float) * 6);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    // Enviar los top 5 y el RMS
    float data_top5[5 * 6];
    for (int i = 0; i < 5; i++) {
        data_top5[i + 5 * 0] = top5[0][i];
        data_top5[i + 5 * 1] = top5[1][i];
        data_top5[i + 5 * 2] = top5[2][i];
        data_top5[i + 5 * 3] = top5[3][i];
        data_top5[i + 5 * 4] = top5[4][i];
        data_top5[i + 5 * 5] = top5[5][i];
    }
    // Enviar de a 5 valores el top 5
    for (int i = 0; i < 6; i++) {
        uart_write_bytes(UART_NUM, (const char *)&data_top5[i * 5], sizeof(float) * 5);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    float data_rms[6];
    data_rms[0] = rms_acc_x;
    data_rms[1] = rms_acc_y;
    data_rms[2] = rms_acc_z;
    data_rms[3] = rms_gyro_x;
    data_rms[4] = rms_gyro_y;
    data_rms[5] = rms_gyro_z;
    uart_write_bytes(UART_NUM, (const char *)data_rms, sizeof(float) * 6);
    vTaskDelay(pdMS_TO_TICKS(1000));

    for (int i = 0; i < window; i++) {
        float fft[12];
        fft[0] = acc_x_fft->re_array[i];
        fft[1] = acc_x_fft->im_array[i];
        fft[2] = acc_y_fft->re_array[i];
        fft[3] = acc_y_fft->im_array[i];
        fft[4] = acc_z_fft->re_array[i];
        fft[5] = acc_z_fft->im_array[i];
        fft[6] = gyro_x_fft->re_array[i];
        fft[7] = gyro_x_fft->im_array[i];
        fft[8] = gyro_y_fft->re_array[i];
        fft[9] = gyro_y_fft->im_array[i];
        fft[10] = gyro_z_fft->re_array[i];
        fft[11] = gyro_z_fft->im_array[i];
        uart_write_bytes(UART_NUM, (const char *)fft, sizeof(float) * 12);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

int HAS_SENT_WINDOW_YET = 0;

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
            bmi_data *data = bmi_read_data(window, &n_reads);
            if (data == NULL) {
                printf("Error al leer datos\n");
                break;
            }

            // Extraer los 5 mayores valores
            float **top5 = bmi_extraer_top_5(data, n_reads);
            if (top5 == NULL) {
                printf("Error al extraer los 5 mayores valores\n");
                free(data);
                break;
            }

            // Inicializa variables que almacenan métricas.
            float rms_acc_x = 0.0, rms_acc_y = 0.0, rms_acc_z = 0.0, rms_gyro_x = 0.0, rms_gyro_y = 0.0, rms_gyro_z = 0.0;
            WindowFFT acc_x_fft = allocate_window_FFT(n_reads);
            WindowFFT acc_y_fft = allocate_window_FFT(n_reads);
            WindowFFT acc_z_fft = allocate_window_FFT(n_reads);
            WindowFFT gyro_x_fft = allocate_window_FFT(n_reads);
            WindowFFT gyro_y_fft = allocate_window_FFT(n_reads);
            WindowFFT gyro_z_fft = allocate_window_FFT(n_reads);

            // TODO: ajustar calcula metricas a bmi
            bmi_calcula_metricas(data, n_reads,
                                 &rms_acc_x, &rms_acc_y, &rms_acc_z, &rms_gyro_x, &rms_gyro_y, &rms_gyro_z,
                                 &acc_x_fft, &acc_y_fft, &acc_z_fft, &gyro_x_fft, &gyro_y_fft, &gyro_z_fft);

            // Enviar los datos al controlador
            bmi_data_sender(data, top5,
                            rms_acc_x, rms_acc_y, rms_acc_z, rms_gyro_x, rms_gyro_y, rms_gyro_z,
                            &acc_x_fft, &acc_y_fft, &acc_z_fft, &gyro_x_fft, &gyro_y_fft, &gyro_z_fft,
                            window);

            // Liberar memoria
            deallocate_window_FFT(acc_x_fft);
            deallocate_window_FFT(acc_y_fft);
            deallocate_window_FFT(acc_z_fft);
            deallocate_window_FFT(gyro_x_fft);
            deallocate_window_FFT(gyro_y_fft);
            deallocate_window_FFT(gyro_z_fft);
            free(data);
            free(top5[0]);
            free(top5[1]);
            free(top5[2]);
            free(top5[3]);
            free(top5[4]);
            free(top5[5]);
            free(top5);

            break;
        case 1:
            write_window_nvs(body);
            // printf("Ventana cambiada a: %ld\n", body);
            break;
        case 2:
            // printf("Cerrando comunicación\n");
            // uart_wait_tx_done(UART_NUM)
            uart_flush(UART_NUM);
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
    // Inicializar BMI
    ESP_ERROR_CHECK(bmi_init());
    bmi_softreset();
    bmi_get_chipid();
    bmi_initialization();
    bmi_check_initialization();
    bmi_powermode();
    bmi_internal_status();

    char signal_buffer[5];
    uart_flush(UART_NUM);

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

        uart_flush(UART_NUM);
        // command_handler(signal_type, *signal_body);
        command_handler(signal_type, signal_body);
        vTaskDelay(pdMS_TO_TICKS(1000));
        // fflush(stdout);
    }
}
