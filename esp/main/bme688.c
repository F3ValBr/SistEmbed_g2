#include <float.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <limits.h>

#include "driver/i2c.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "math.h"
#include "sdkconfig.h"
#include "nvs_flash.h"
#include "nvs.h"

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

#define REDIRECT_LOGS 1 // if redirect ESP log to another UART

esp_err_t ret = ESP_OK;
esp_err_t ret2 = ESP_OK;

uint16_t val0[6];

float task_delay_ms = 1000;

esp_err_t sensor_init(void) {
    int i2c_master_port = I2C_NUM_0;
    i2c_config_t conf;
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_DISABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_DISABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    conf.clk_flags = I2C_SCLK_SRC_FLAG_FOR_NOMAL;  // 0
    i2c_param_config(i2c_master_port, &conf);
    return i2c_driver_install(i2c_master_port, conf.mode, 0, 0, 0);
}

esp_err_t bme_i2c_read(i2c_port_t i2c_num, uint8_t *data_addres, uint8_t *data_rd, size_t size) {
    if (size == 0) {
        return ESP_OK;
    }
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME_ESP_SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write(cmd, data_addres, size, ACK_CHECK_EN);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME_ESP_SLAVE_ADDR << 1) | READ_BIT, ACK_CHECK_EN);
    if (size > 1) {
        i2c_master_read(cmd, data_rd, size - 1, ACK_VAL);
    }
    i2c_master_read_byte(cmd, data_rd + size - 1, NACK_VAL);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

esp_err_t bme_i2c_write(i2c_port_t i2c_num, uint8_t *data_addres, uint8_t *data_wr, size_t size) {
    uint8_t size1 = 1;
    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (BME_ESP_SLAVE_ADDR << 1) | WRITE_BIT, ACK_CHECK_EN);
    i2c_master_write(cmd, data_addres, size1, ACK_CHECK_EN);
    i2c_master_write(cmd, data_wr, size, ACK_CHECK_EN);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(i2c_num, cmd, 1000 / portTICK_PERIOD_MS);
    i2c_cmd_link_delete(cmd);
    return ret;
}

// ------------ BME 688 ------------- //
uint8_t calc_gas_wait(uint16_t dur) {
    // Fuente: BME688 API
    // https://github.com/boschsensortec/BME68x_SensorAPI/blob/master/bme68x.c#L1176
    uint8_t factor = 0;
    uint8_t durval;

    if (dur >= 0xfc0) {
        durval = 0xff; /* Max duration*/
    } else {
        while (dur > 0x3F) {
            dur = dur >> 2;
            factor += 1;
        }

        durval = (uint8_t)(dur + (factor * 64));
    }

    return durval;
}

uint8_t calc_res_heat(uint16_t temp) {
    // Fuente: BME688 API
    // https://github.com/boschsensortec/BME68x_SensorAPI/blob/master/bme68x.c#L1145
    uint8_t heatr_res;
    uint8_t amb_temp = 25;

    uint8_t reg_par_g1 = 0xED;
    uint8_t par_g1;
    bme_i2c_read(I2C_NUM_0, &reg_par_g1, &par_g1, 1);

    uint8_t reg_par_g2_lsb = 0xEB;
    uint8_t par_g2_lsb;
    bme_i2c_read(I2C_NUM_0, &reg_par_g2_lsb, &par_g2_lsb, 1);
    uint8_t reg_par_g2_msb = 0xEC;
    uint8_t par_g2_msb;
    bme_i2c_read(I2C_NUM_0, &reg_par_g2_msb, &par_g2_msb, 1);
    uint16_t par_g2 = (int16_t)(CONCAT_BYTES(par_g2_msb, par_g2_lsb));

    uint8_t reg_par_g3 = 0xEE;
    uint8_t par_g3;
    bme_i2c_read(I2C_NUM_0, &reg_par_g3, &par_g3, 1);

    uint8_t reg_res_heat_range = 0x02;
    uint8_t res_heat_range;
    uint8_t mask_res_heat_range = (0x3 << 4);
    uint8_t tmp_res_heat_range;

    uint8_t reg_res_heat_val = 0x00;
    uint8_t res_heat_val;

    int32_t var1;
    int32_t var2;
    int32_t var3;
    int32_t var4;
    int32_t var5;
    int32_t heatr_res_x100;

    if (temp > 400) {
        temp = 400;
    }

    bme_i2c_read(I2C_NUM_0, &reg_res_heat_range, &tmp_res_heat_range, 1);
    bme_i2c_read(I2C_NUM_0, &reg_res_heat_val, &res_heat_val, 1);
    res_heat_range = (mask_res_heat_range & tmp_res_heat_range) >> 4;

    var1 = (((int32_t)amb_temp * par_g3) / 1000) * 256;
    var2 = (par_g1 + 784) * (((((par_g2 + 154009) * temp * 5) / 100) + 3276800) / 10);
    var3 = var1 + (var2 / 2);
    var4 = (var3 / (res_heat_range + 4));
    var5 = (131 * res_heat_val) + 65536;
    heatr_res_x100 = (int32_t)(((var4 / var5) - 250) * 34);
    heatr_res = (uint8_t)((heatr_res_x100 + 50) / 100);

    return heatr_res;
}

int bme_get_chipid(void) {
    uint8_t reg_id = 0xd0;
    uint8_t tmp;

    bme_i2c_read(I2C_NUM_0, &reg_id, &tmp, 1);
    printf("Valor de CHIPID: %2X \n\n", tmp);

    if (tmp == 0x61) {
        printf("Chip BME688 reconocido.\n\n");
        return 0;
    } else {
        printf("Chip BME688 no reconocido. \nCHIP ID: %2x\n\n", tmp);  // %2X
    }

    return 1;
}

int bme_softreset(void) {
    uint8_t reg_softreset = 0xE0, val_softreset = 0xB6;

    ret = bme_i2c_write(I2C_NUM_0, &reg_softreset, &val_softreset, 1);
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        printf("\nError en softreset: %s \n", esp_err_to_name(ret));
        return 1;
    } else {
        printf("\nSoftreset: OK\n\n");
    }
    return 0;
}

void bme_forced_mode(void) {
    /*
    Fuente: Datasheet[19]
    https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme688-ds000.pdf#page=19

    Para configurar el BME688 en forced mode los pasos son:

    1. Set humidity oversampling to 1x     |-| 0b001 to osrs_h<2:0>
    2. Set temperature oversampling to 2x  |-| 0b010 to osrs_t<2:0>
    3. Set pressure oversampling to 16x    |-| 0b101 to osrs_p<2:0>

    4. Set gas duration to 100 ms          |-| 0x59 to gas_wait_0
    5. Set heater step size to 0           |-| 0x00 to res_heat_0
    6. Set number of conversion to 0       |-| 0b0000 to nb_conv<3:0> and enable gas measurements
    7. Set run_gas to 1                    |-| 0b1 to run_gas<5>

    8. Set operation mode                  |-| 0b01  to mode<1:0>

    */

    // Datasheet[33]
    uint8_t ctrl_hum = 0x72;
    uint8_t ctrl_meas = 0x74;
    uint8_t gas_wait_0 = 0x64;
    uint8_t res_heat_0 = 0x5A;
    uint8_t ctrl_gas_1 = 0x71;

    uint8_t mask;
    uint8_t prev;
    // Configuramos el oversampling (Datasheet[36])

    // 1. osrs_h esta en ctrl_hum (LSB) -> seteamos 001 en bits 2:0
    uint8_t osrs_h = 0b001;
    mask = 0b00000111;
    bme_i2c_read(I2C_NUM_0, &ctrl_hum, &prev, 1);
    osrs_h = (prev & ~mask) | osrs_h;

    // 2. osrs_t esta en ctrl_meas MSB -> seteamos 010 en bits 7:5
    uint8_t osrs_t = 0b01000000;
    // 3. osrs_p esta en ctrl_meas LSB -> seteamos 101 en bits 4:2 [Datasheet:37]
    uint8_t osrs_p = 0b00010100;
    uint8_t osrs_t_p = osrs_t | osrs_p;
    // Se recomienda escribir hum, temp y pres en un solo write

    // Configuramos el sensor de gas

    // 4. Seteamos gas_wait_0 a 100ms
    uint8_t gas_duration = calc_gas_wait(100);

    // 5. Seteamos res_heat_0
    uint8_t heater_step = calc_res_heat(300);

    // 6. nb_conv esta en ctrl_gas_1 -> seteamos bits 3:0
    uint8_t nb_conv = 0b00000000;
    // 7. run_gas esta en ctrl_gas_1 -> seteamos bit 5
    uint8_t run_gas = 0b00100000;
    uint8_t gas_conf = nb_conv | run_gas;

    bme_i2c_write(I2C_NUM_0, &gas_wait_0, &gas_duration, 1);
    bme_i2c_write(I2C_NUM_0, &res_heat_0, &heater_step, 1);
    bme_i2c_write(I2C_NUM_0, &ctrl_hum, &osrs_h, 1);
    bme_i2c_write(I2C_NUM_0, &ctrl_meas, &osrs_t_p, 1);
    bme_i2c_write(I2C_NUM_0, &ctrl_gas_1, &gas_conf, 1);

    // Seteamos el modo
    // 8. Seteamos el modo a 01, pasando primero por sleep
    uint8_t mode = 0b00000001;
    uint8_t tmp_pow_mode;
    uint8_t pow_mode = 0;

    do {
        ret = bme_i2c_read(I2C_NUM_0, &ctrl_meas, &tmp_pow_mode, 1);

        if (ret == ESP_OK) {
            // Se pone en sleep
            pow_mode = (tmp_pow_mode & 0x03);
            if (pow_mode != 0) {
                tmp_pow_mode &= ~0x03;
                ret = bme_i2c_write(I2C_NUM_0, &ctrl_meas, &tmp_pow_mode, 1);
            }
        }
    } while ((pow_mode != 0x0) && (ret == ESP_OK));

    tmp_pow_mode = (tmp_pow_mode & ~0x03) | mode;
    ret = bme_i2c_write(I2C_NUM_0, &ctrl_meas, &tmp_pow_mode, 1);
    vTaskDelay(pdMS_TO_TICKS(50));
}

int bme_check_forced_mode(void) {
    uint8_t ctrl_hum = 0x72;
    uint8_t ctrl_meas = 0x74;
    uint8_t gas_wait_0 = 0x64;
    uint8_t res_heat_0 = 0x5A;
    uint8_t ctrl_gas_1 = 0x71;

    uint8_t tmp, tmp2, tmp3, tmp4, tmp5;

    ret = bme_i2c_read(I2C_NUM_0, &ctrl_hum, &tmp, 1);
    ret = bme_i2c_read(I2C_NUM_0, &gas_wait_0, &tmp2, 1);
    ret = bme_i2c_read(I2C_NUM_0, &res_heat_0, &tmp3, 1);
    ret = bme_i2c_read(I2C_NUM_0, &ctrl_gas_1, &tmp4, 1);
    ret = bme_i2c_read(I2C_NUM_0, &ctrl_meas, &tmp5, 1);
    vTaskDelay(task_delay_ms / portTICK_PERIOD_MS);
    return (tmp == 0b001 && tmp2 == 0x59 && tmp3 == 0x00 && tmp4 == 0b100000 && tmp5 == 0b01010101);
}

int t_fine;

int bme_temp_celsius(uint32_t temp_adc) {
    // Datasheet[23]
    // https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme688-ds000.pdf#page=23

    // Se obtienen los parametros de calibracion
    uint8_t addr_par_t1_lsb = 0xE9, addr_par_t1_msb = 0xEA;
    uint8_t addr_par_t2_lsb = 0x8A, addr_par_t2_msb = 0x8B;
    uint8_t addr_par_t3_lsb = 0x8C;
    uint16_t par_t1;
    uint16_t par_t2;
    uint16_t par_t3;

    uint8_t par[5];
    bme_i2c_read(I2C_NUM_0, &addr_par_t1_lsb, par, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_t1_msb, par + 1, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_t2_lsb, par + 2, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_t2_msb, par + 3, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_t3_lsb, par + 4, 1);

    par_t1 = (par[1] << 8) | par[0];
    par_t2 = (par[3] << 8) | par[2];
    par_t3 = par[4];

    int64_t var1;
    int64_t var2;
    int64_t var3;
    int calc_temp;

    var1 = ((int32_t)temp_adc >> 3) - ((int32_t)par_t1 << 1);
    var2 = (var1 * (int32_t)par_t2) >> 11;
    var3 = ((var1 >> 1) * (var1 >> 1)) >> 12;
    var3 = ((var3) * ((int32_t)par_t3 << 4)) >> 14;
    t_fine = (int32_t)(var2 + var3);
    calc_temp = (((t_fine * 5) + 128) >> 8);
    return calc_temp;
}

int bme_pres_pa(uint32_t pres_adc) {
    // Datasheet[23]
    // https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme688-ds000.pdf#page=23

    // Se obtienen los parametros de calibracion
    uint8_t addr_par_p1_lsb = 0x8E, addr_par_p1_msb = 0x8F;
    uint8_t addr_par_p2_lsb = 0x90, addr_par_p2_msb = 0x91;
    uint8_t addr_par_p3 = 0x92;
    uint8_t addr_par_p4_lsb = 0x94, addr_par_p4_msb = 0x95;
    uint8_t addr_par_p5_lsb = 0x96, addr_par_p5_msb = 0x97;
    uint8_t addr_par_p6 = 0x99;
    uint8_t addr_par_p7 = 0x98;
    uint8_t addr_par_p8_lsb = 0x9C, addr_par_p8_msb = 0x9D;
    uint8_t addr_par_p9_lsb = 0x9E, addr_par_p9_msb = 0x9F;
    uint8_t addr_par_p10 = 0xA0;

    uint16_t par_p1;
    uint16_t par_p2;
    uint16_t par_p3;
    uint16_t par_p4;
    uint16_t par_p5;
    uint16_t par_p6;
    uint16_t par_p7;
    uint16_t par_p8;
    uint16_t par_p9;
    uint16_t par_p10;

    uint8_t par[16];
    bme_i2c_read(I2C_NUM_0, &addr_par_p1_lsb, par, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p1_msb, par + 1, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p2_lsb, par + 2, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p2_msb, par + 3, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p3, par + 4, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p4_lsb, par + 5, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p4_msb, par + 6, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p5_lsb, par + 7, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p5_msb, par + 8, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p6, par + 9, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p7, par + 10, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p8_lsb, par + 11, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p8_msb, par + 12, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p9_lsb, par + 13, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p9_msb, par + 14, 1);
    bme_i2c_read(I2C_NUM_0, &addr_par_p10, par + 15, 1);

    par_p1 = (par[1] << 8) | par[0];
    par_p2 = (par[3] << 8) | par[2];
    par_p3 = par[4];
    par_p4 = (par[6] << 8) | par[5];
    par_p5 = (par[8] << 8) | par[7];
    par_p6 = par[9];
    par_p7 = par[10];
    par_p8 = (par[12] << 8) | par[11];
    par_p9 = (par[14] << 8) | par[13];
    par_p10 = par[15];

    int64_t var1;
    int64_t var2;
    int64_t var3;
    int calc_pres;

    // Esto es copiado y pegado de la documentacion
    var1 = ((int32_t)t_fine >> 1) - 64000;
    var2 = ((((var1 >> 2) * (var1 >> 2)) >> 11) * (int32_t)par_p6) >> 2;
    var2 = var2 + ((var1 * (int32_t)par_p5) << 1);
    var2 = (var2 >> 2) + ((int32_t)par_p4 << 16);
    var1 = (((((var1 >> 2) * (var1 >> 2)) >> 13) * ((int32_t)par_p3 << 5)) >> 3) + (((int32_t)par_p2 * var1) >> 1);
    var1 = var1 >> 18;
    var1 = ((32768 + var1) * (int32_t)par_p1) >> 15;
    calc_pres = 1048576 - pres_adc;
    calc_pres = (int)((calc_pres - (var2 >> 12)) * ((uint32_t)3125));
    if (calc_pres >= (1 << 30)) {
        calc_pres = ((calc_pres / (uint32_t)var1) << 1);
    } else {
        calc_pres = ((calc_pres << 1) / (uint32_t)var1);
    }
    var1 = ((int32_t)par_p9 * (int32_t)(((calc_pres >> 3) * (calc_pres >> 3)) >> 13)) >> 12;
    var2 = ((int32_t)(calc_pres >> 2) * (int32_t)par_p8) >> 13;
    var3 = ((int32_t)(calc_pres >> 8) * (int32_t)(calc_pres >> 8) * (int32_t)(calc_pres >> 8) * (int32_t)par_p10) >> 17;
    calc_pres = (int32_t)(calc_pres) + ((var1 + var2 + var3 + ((int32_t)par_p7 << 7)) >> 4);
    return calc_pres;
}

void bme_get_mode(void) {
    uint8_t reg_mode = 0x74;
    uint8_t tmp;

    ret = bme_i2c_read(I2C_NUM_0, &reg_mode, &tmp, 1);

    tmp = tmp & 0x3;

    printf("Valor de BME MODE: %2X \n\n", tmp);
}

// bme_data es una estructura que contiene la temperatura y la presion en un instante de tiempo
typedef struct bme_data {
    int temperature;
    int presure;
} bme_data;

bme_data *bme_read_data(int window_s, size_t *n_reads) {
    // Datasheet[23:41]
    // https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme688-ds000.pdf#page=23

    uint8_t tmp;
    *n_reads = 0;

    // Se obtienen los datos de temperatura
    uint8_t forced_temp_addr[] = {0x22, 0x23, 0x24};
    uint8_t forced_pres_addr[] = {0x1F, 0x20, 0x21};

    bme_data *readings = malloc(window_s * sizeof(bme_data));
    if (readings == NULL) {
        printf("Error al asignar memoria\n");
        return NULL;
    }

    while (*n_reads < window_s) {
        uint32_t temp_adc = 0;
        bme_forced_mode();
        // Datasheet[41]
        // https://www.bosch-sensortec.com/media/boschsensortec/downloads/datasheets/bst-bme688-ds000.pdf#page=41

        bme_i2c_read(I2C_NUM_0, &forced_temp_addr[0], &tmp, 1);
        temp_adc = temp_adc | tmp << 12;
        bme_i2c_read(I2C_NUM_0, &forced_temp_addr[1], &tmp, 1);
        temp_adc = temp_adc | tmp << 4;
        bme_i2c_read(I2C_NUM_0, &forced_temp_addr[2], &tmp, 1);
        temp_adc = temp_adc | (tmp & 0xf0) >> 4;

        uint32_t temp = bme_temp_celsius(temp_adc);
        printf("Temperatura: %f\n", (float)temp / 100);

        // Se obtienen los datos de presion
        uint32_t pres_adc = 0;
        bme_i2c_read(I2C_NUM_0, &forced_pres_addr[0], &tmp, 1);
        pres_adc = pres_adc | tmp << 12;
        bme_i2c_read(I2C_NUM_0, &forced_pres_addr[1], &tmp, 1);
        pres_adc = pres_adc | tmp << 4;
        bme_i2c_read(I2C_NUM_0, &forced_pres_addr[2], &tmp, 1);
        pres_adc = pres_adc | (tmp & 0xf0) >> 4;

        uint32_t pres = bme_pres_pa(pres_adc);
        printf("Presion: %f\n", (float)pres / 100);

        bme_data data;
        data.temperature = temp;
        data.presure = pres;

        readings[*n_reads] = data;
        (*n_reads)++;
    }

    return readings;
}

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
        top5[1][i] = -FLT_MAX;
    }

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

// Lectura/escritura de datos en la NVM
void init_nvs() {
    // Initialize NVS
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        // NVS partition was truncated and needs to be erased
        // Retry nvs_flash_init
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
}

void write_window_nvs(int window) {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK) {
        printf("Error (%s) abriendo el NVS handler!\n", esp_err_to_name(err));
    } else {
        printf("NVS abierta! Guardando la ventana: %d\n", window);
        // Escribir el valor de la ventana
        err = nvs_set_i32(my_handle, "window", window);
        printf((err != ESP_OK) ? "Fallo!\n" : "Hecho! Ventana guardada\n");
        // Commit written value.
        err = nvs_commit(my_handle);
        printf((err != ESP_OK) ? "Fallo!\n" : "Hecho! Cambios guardados\n");
        // Cerrar la NVS
        nvs_close(my_handle);
    }
}

int32_t read_window_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    int32_t window = 20;  // Valor por defecto
    if (err != ESP_OK) {
        printf("Error (%s) abriendo el NVS handler!\n", esp_err_to_name(err));
    } else {
        printf("NVS abierta! Leyendo la ventana\n");
        // Leer el valor de la ventana
        err = nvs_get_i32(my_handle, "window", &window);
        switch (err) {
            case ESP_OK:
                printf("Ventana almacenada: %ld\n", window);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                printf("La ventana no ha sido guardada todavia!\n");
                break;
            default:
                printf("Error (%s) leyendo!\n", esp_err_to_name(err));
        }
        // Cerrar la NVS
        nvs_close(my_handle);
    }
    return window;
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
int serial_write(const char *msg, int len) {
    char *send_with_end = (char *)malloc(sizeof(char) * (len + 1));
    memcpy(send_with_end, msg, len);
    send_with_end[len] = '\0';

    int result = uart_write_bytes(UART_NUM, send_with_end, len + 1);

    free(send_with_end);

    vTaskDelay(pdMS_TO_TICKS(1000));  // Delay for 1 second
    return result;
}

// CAMBIAR PARA QUE CONCUERDE CON PYTHON
void command_handler(uint8_t signal_type, uint32_t body) {
    switch (signal_type) {
        case 0:
            // Extraer la ventana
            int32_t window = read_window_nvs();
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
            uart_write_bytes(UART_NUM, (const char *)window, sizeof(int32_t));
            vTaskDelay(pdMS_TO_TICKS(50));

            // ventana
            uart_write_bytes(UART_NUM, (const char *)data, sizeof(bme_data));
            vTaskDelay(pdMS_TO_TICKS(50));

            // top 5 y RMS
            float pkg[12];
            for (int i = 0; i < 5;) {
                pkg[i] = top5[0][i];
                pkg[i + 5] = top5[1][i];
            }
            pkg[10] = rms_temp;
            pkg[11] = rms_pres;
            uart_write_bytes(UART_NUM, (const char *)pkg, sizeof(float) * 12);
            vTaskDelay(pdMS_TO_TICKS(50));
            // serial_write((const char *)pkg, sizeof(float) * 12);

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
    bme_softreset(); // Soft reset

    char signal_buffer[5];
    while (true) {
        // Espera señal
        if (serial_read(signal_buffer, 5) == 0) {
            continue;
        }
        // Reacciona al tipo de señal
        uint8_t signal_type = signal_buffer[0];

        uint32_t *signal_body = &signal_buffer[1];
        command_handler(signal_type, *signal_body);
    }
}
