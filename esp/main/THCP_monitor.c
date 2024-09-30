#include "embebidos/THCP_monitor.h"

WindowFFT allocate_window_FFT(int size) {
    WindowFFT new_instance;
    new_instance.re_array = malloc(sizeof(float) * size);
    new_instance.im_array = malloc(sizeof(float) * size);
    return new_instance;
}

void deallocate_window_FFT(WindowFFT target_window) {
    free(target_window.re_array);
    free(target_window.im_array);
}

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