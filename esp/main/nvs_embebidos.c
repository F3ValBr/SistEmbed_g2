#include "embebidos/nvs_embebidos.h"

#include "nvs.h"
#include "nvs_flash.h"

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
        //printf("Error (%s) abriendo el NVS handler!\n", esp_err_to_name(err));
    } else {
        //printf("NVS abierta! Guardando la ventana: %d\n", window);
        // Escribir el valor de la ventana
        err = nvs_set_i32(my_handle, "window", window);
        //printf((err != ESP_OK) ? "Fallo!\n" : "Hecho! Ventana guardada\n");
        // Commit written value.
        err = nvs_commit(my_handle);
        //printf((err != ESP_OK) ? "Fallo!\n" : "Hecho! Cambios guardados\n");
        // Cerrar la NVS
        nvs_close(my_handle);
    }
}

int32_t read_window_nvs() {
    nvs_handle_t my_handle;
    esp_err_t err = nvs_open("storage", NVS_READONLY, &my_handle);
    int32_t window = 20;  // Valor por defecto
    if (err != ESP_OK) {
        //printf("Error (%s) abriendo el NVS handler!\n", esp_err_to_name(err));
    } else {
        //printf("NVS abierta! Leyendo la ventana\n");
        // Leer el valor de la ventana
        err = nvs_get_i32(my_handle, "window", &window);
        switch (err) {
            case ESP_OK:
                //printf("Ventana almacenada: %ld\n", window);
                break;
            case ESP_ERR_NVS_NOT_FOUND:
                int32_t window = 20;
                //printf("La ventana no ha sido guardada todavia!\n");
                break;
            default:
                break;
                //printf("Error (%s) leyendo!\n", esp_err_to_name(err));
        }
        // Cerrar la NVS
        nvs_close(my_handle);
    }
    return window;
}