Para la ejecucion, lo relacionado a la ESP32 (el codigo de C) se encuentra en la carpeta "esp".
El comando de ejecucion del build se hizo desde dentro de la misma.

Para la ejecucion del programa python, se uso el siguiente comando:

```
python cliente/main.py
```

Con esto, el programa entra en la seleccion de opciones, donde:

- 0 es para solicitar una ventana
- 1 es para cambiar la ventana en memoria
- 2 es para terminar el programa y reiniciar la ESP

# Definición de puerto serial para cliente de Python

Han de definir un archivo `.env` en la raíz del proyecto, definiendo la variable `SERIAL_PORT` que indique el puerto que usará al ejecutar, con el siguiente formato por ejemplo:

    # .env
    SERIAL_PORT=/dev/ttyUSB0
