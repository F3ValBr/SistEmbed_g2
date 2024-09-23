#pragma once
#include "driver/i2c.h"
#include "esp_err.h"

// bme_data es una estructura que contiene la temperatura y la presion en un instante de tiempo
typedef struct bme_data {
    float temperature;
    float presure;
    float humidity;
} bme_data;

esp_err_t sensor_init(void);
esp_err_t bme_i2c_read(i2c_port_t i2c_num, uint8_t *data_addres, uint8_t *data_rd, size_t size);
esp_err_t bme_i2c_write(i2c_port_t i2c_num, uint8_t *data_addres, uint8_t *data_wr, size_t size);

// ------------ BME 688 ------------- //
uint8_t calc_gas_wait(uint16_t dur);
uint8_t calc_res_heat(uint16_t temp);
int bme_get_chipid(void);
int bme_softreset(void);
void bme_forced_mode(void);
int bme_check_forced_mode(void);
int bme_temp_celsius(uint32_t temp_adc);
void bme_get_mode(void);
void bme_read_data_example(void);
bme_data *bme_read_data(int window_s, size_t *n_reads);
