from struct import unpack, pack
from serial import Serial
from time import sleep
import numpy as np
import matplotlib.pyplot as plt


class MissingWindowSizeError(Exception):
    pass


class Controller:
    running = True
    first_execution = True
    window_data = []
    temp_top = None
    pres_top = None
    temp_rms = None
    pres_rms = None

    def __init__(self, serial_port=None):
        self.window_size = 10
        self.ser: Serial = serial_port

    def receive_response(self):
        response = self.ser.readline()
        return response

    def send_message(self, message):
        """Funcion para enviar un mensaje a la ESP32"""
        self.ser.write(message)

    def request_window(self):
        self.send_message(self.pack_signal(0))
        self.window_data = []

    def receive_bme_data(self):
        data = self.ser.read(12)
        print(f"len data {len(data)}")
        data = unpack('fff', data)
        return data
    
    def receive_measures(self):
        data = self.ser.read(48)
        data = unpack("5f5fff", data)
        return data

    def get_window(self):
        print("Solicitando datos...")
        sleep(10)

        print("Recibiendo datos...")
        message = pack('6s','BEGIN\0'.encode())
        self.send_message(message)

        counter = 0
        while True:
            if self.ser.in_waiting > 11:
                try:
                    obt_data = self.receive_bme_data()
                    print(obt_data)
                    self.window_data.append(obt_data)
                    if self.first_execution and len(self.window_data) == 1:
                        self.window_size = int(obt_data[2])
                        self.first_execution = False
                except Exception as e:
                    print(e)
                    continue
                else:
                    counter += 1
                    print(f"Contador data: {counter}")
                finally:
                    if counter == self.window_size:
                        print("Lecturas obtenidas")
                        break
        print("Datos obtenidos")
        print(self.window_data)
        sleep(1)
        print("Obteniendo medidas adicionales...")
        counter_measures = 0
        
        while True:
            if self.ser.in_waiting > 47:
                try:
                    m_data = self.receive_measures()
                    print(m_data)
                    self.window_data.append(m_data)
                except Exception as e:
                    print(e)
                    continue
                else:
                    counter_measures += 1
                    print(f"Contador data: {counter_measures}")
                finally:
                    if counter_measures == 1:
                        print("Medidas obtenidas")
                        break

        print(self.window_data)
        
            # TODO: Actualizar visualización

        # TODO: visualize data

    def get_window_2(self):
        while self.ser.in_waiting < 4:
            pass
        window_size = int.from_bytes(self.ser.read(4), 'little')
        tmp_data = []
        tmp_temp_top = []
        tmp_pres_top = []
        tmp_temp_rms = 0
        tmp_pres_rms = 0

        for _ in range(window_size):
            while self.ser.in_waiting < 8:
                pass
            new_temp = unpack('f', self.ser.read(4))[0]
            new_pres = unpack('f', self.ser.read(4))[0]
            tmp_data.append((new_temp, new_pres))

        while self.ser.in_waiting < 12 * 4:
            pass

        values = unpack('<5f5fff', self.ser.read(12 * 4))
        tmp_temp_top = list(values[0:5])
        tmp_pres_top = list(values[5:10])
        tmp_temp_rms = values[10]
        tmp_pres_rms = values[11]

        self.window_data = tmp_data
        self.temp_top = tmp_temp_top
        self.pres_top = tmp_pres_top
        self.temp_rms = tmp_temp_rms
        self.pres_rms = tmp_pres_rms

        # TODO: visualize data

    def change_window_size(self, new_window_size):
        try:
            new_window_size = int(new_window_size)
            self.send_message(self.pack_signal(1, new_window_size))
            self.window_size = new_window_size
            print(
                f"El tamaño de la ventana se ha cambiado a {new_window_size}")
        except ValueError:
            print("Tamano de ventana invalido")

    def shutdown(self):
        self.running = False
        self.send_message(self.pack_signal(2))
        print("La ESP se esta reiniciando...")

    def close(self):
        self.ser.close()

    def pack_signal(self, signal_type: int, body: int | None = None) -> bytes:
        if body is None:
            if signal_type == 1:
                raise MissingWindowSizeError()
            return signal_type.to_bytes(1, 'little')
        return signal_type.to_bytes(1, 'little') + body.to_bytes(4, 'little')
