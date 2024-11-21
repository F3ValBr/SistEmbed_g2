#pragma once
#include "esp_err.h"

// bmi_data es una estructura que contiene la aceleración y el giroscopio en un instante de tiempo
typedef struct bmi_data {
    float acc_x;
    float acc_y;
    float acc_z;
    float gyro_x;
    float gyro_y;
    float gyro_z;
} bmi_data;

bmi_data *bmi_read_data(int window_s, size_t *n_reads);
esp_err_t bmi_init(void);
void bmi_softreset(void);
void bmi_get_chipid(void);
void bmi_initialization(void);
void bmi_check_initialization(void);
void bmi_powermode(void);
void bmi_internal_status(void);
