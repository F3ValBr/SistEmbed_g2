Esta es el proyecto relacionado a Tarea 4 Opción A, sobre la creación de un programa de medición con el BMI270 usando PyQt5 como interfaz.

Para la ejecucion, lo relacionado a la ESP32 (el codigo de C) se encuentra en la carpeta "esp".
El comando de ejecucion del build se hizo desde dentro de la misma. En caso de presentar algún fallo, borrar los archivos relacionados a la build y volver a ejecutar.

Para la ejecucion del programa python, se uso el siguiente comando:

```
python cliente/t4_ventana.py
```

Con esto, el programa mostrará una ventana con tres opciones, donde:

- El primer botón es para cambiar la ventana en memoria, con una zona para señalar dicha ventana
- El segundo botón es para solicitar una ventana usando la almacenada en memoria NVS
- El tercer botón es para terminar el programa y reiniciar la ESP

La solicitud de la ventana tarda en la medida del tamaño de la ventana almacenado, se imprimiran en la consola que ejecuta la ventana los datos en la medida que son recepcionados por python

# Definición de puerto serial para cliente de Python

Ha de definirse un archivo `.env` en la raíz del proyecto, definiendo la variable `SERIAL_PORT` que indique el puerto que usará al ejecutar, con el siguiente formato por ejemplo:

    # .env
    SERIAL_PORT=/dev/ttyUSB0
