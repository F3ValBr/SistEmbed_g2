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
void fill_bmi_arrays(float *acc_x,
                        float *acc_y,
                        float *acc_z,
                        float *gyro_x,
                        float *gyro_y,
                        float *gyro_z,
                        bmi_data *data, 
                        int window_size) {
    for (int i = 0; i < window_size; i++) {
        acc_x[i] = data[i].acc_x;
        acc_y[i] = data[i].acc_y;
        acc_z[i] = data[i].acc_z;
        gyro_x[i] = data[i].gyro_x;
        gyro_y[i] = data[i].gyro_y;
        gyro_z[i] = data[i].gyro_z;
    }
}