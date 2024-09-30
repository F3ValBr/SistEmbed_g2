#pragma once
#include "embebidos/bme.h"

typedef struct WindowFFT {
    float *re_array;
    float *im_array;
} WindowFFT;

WindowFFT allocate_window_FFT(int size);
void deallocate_window_FFT(WindowFFT target_window);

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
void fill_THCP_arrays(float *temp, float *hum, float *gas, float *pres, bme_data *data, int window_size);