#include "embebidos/checksum.h"

#include <stdio.h>
/**
 * @brief Suma cada byte de un mensaje.
 *
 * @param str Puntero a los bytes a los cuales se quiere calcular checksum.
 * @return unsigned int Valor del checksum.
 */
unsigned int checksum(char *str) {
    unsigned int sum = 0;
    while (*str) {
        sum += *str;
        str++;
    }
    return sum;
}