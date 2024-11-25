#pragma once
#include "embebidos/bmi.h"
#define REDIRECT_LOGS 1  // if redirect ESP log to another UART

typedef struct WindowFFT {
    float *re_array;
    float *im_array;
} WindowFFT;

WindowFFT allocate_window_FFT(int size);
void deallocate_window_FFT(WindowFFT target_window);

/**
 * @brief Separa valores leídos según dato.
 * 
 * @param acc_x Arreglo de aceleraciones en x a llenar.
 * @param acc_y Arreglo de aceleraciones en y a llenar.
 * @param acc_z Arreglo de aceleraciones en z a llenar.
 * @param gyro_x Arreglo de velocidades angulares en x a llenar.
 * @param gyro_y Arreglo de velocidades angulares en y a llenar.
 * @param gyro_z Arreglo de velocidades angulares en z a llenar.
 * @param data Arreglo de origen con datos agrupados.
 * @param window_size Tamaño de arreglo.
 * @return void
 */
void fill_bmi_arrays(
    float *acc_x,
    float *acc_y,
    float *acc_z,
    float *gyro_x,
    float *gyro_y,
    float *gyro_z,
    bmi_data *data, 
    int window_size);