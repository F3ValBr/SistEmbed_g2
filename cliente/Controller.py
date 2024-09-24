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
        data = self.ser.read(16)
        data = unpack('ffff', data)
        return data
    
    def receive_measures(self):
        data = self.ser.read(48)
        data = unpack("5f5fff", data)
        return data
    
    def receive_top(self):
        data = self.ser.read(20)
        data = unpack("5f", data)
        return data
    
    def receive_rms(self):
        data = self.ser.read(12)
        data = unpack("fff", data)
        return data

    def get_window(self):
        print("Solicitando datos...")
        sleep(10)

        print("Recibiendo datos...")
        message = pack('6s','BEGIN\0'.encode())
        self.send_message(message)

        counter = 0
        while True:
            if self.ser.in_waiting > 15:
                try:
                    obt_data = self.receive_bme_data()
                    print(obt_data)
                    self.window_data.append(obt_data)
                    if self.first_execution and len(self.window_data) == 1:
                        self.window_size = int(obt_data[3])
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
        print("Obteniendo medidas - Top 5")
        counter_measures = 0
        
        while True:
            if self.ser.in_waiting > 19:
                try:
                    top_data = self.receive_top()
                    print(top_data)
                    self.window_data.append(top_data)
                except Exception as e:
                    print(e)
                    continue
                else:
                    counter_measures += 1
                    print(f"Contador data: {counter_measures}")
                finally:
                    if counter_measures == 3:
                        print("Medidas del top 5 obtenidas")
                        break
        
        print("Datos obtenidos")
        print(self.window_data)
        sleep(1)
        print("Obteniendo medidas - RMS")
        counter_rms = 0

        while True:
            if self.ser.in_waiting > 11:
                try:
                    rms_data = self.receive_rms()
                    print(rms_data)
                    self.window_data.append(rms_data)
                except Exception as e:
                    print(e)
                    continue
                else:
                    counter_rms += 1
                    print(f"Contador data: {counter_rms}")
                finally:
                    if counter_rms == 1:
                        print("Medidas RMS obtenidas")
                        break

        print("Datos obtenidos")
        print(self.window_data)
        sleep(1)
        print("Graficando...")

        # print(self.window_data)

        self.plot_window()

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

    def plot_window(self):
        data_bme = self.window_data[0:self.window_size]
        data_measurements = self.window_data[-1]
        self.temp_top = data_measurements[0:5]
        self.pres_top = data_measurements[5:10]
        self.temp_rms = data_measurements[10]
        self.pres_rms = data_measurements[11]
        
        data_array = np.array(data_bme)
        data_temp = data_array[:,0]
        data_pres = data_array[:,1]

        temp_points = []
        pres_points = []

        for i in range(5):
            temp_idx = np.where(data_temp == self.temp_top[i])
            temp_points.append((temp_idx, self.temp_top[i]))
            
            pres_idx = np.where(data_pres == self.pres_top[i])

            pres_points.append((pres_idx, self.pres_top[i]))

            


        fig, axs = plt.subplots(1, 2)

        fig.suptitle(f"Temperatura y Presion (ventana: {self.window_size}")

        axs[0].plot(data_temp)
        axs[0].set_title(f"Temperatura (RMS = {self.temp_rms})")
        axs[0].set_ylabel("Grados Celsius (°C)")
        axs[0].set_ylim(0, 50)

        for value in temp_points:
            axs[0].plot(value[0], value[1], 'ro')


        axs[1].plot(data_pres)
        axs[1].set_title(f"Presion (RMS = {self.pres_rms})")
        axs[1].set_ylabel("Pascales (Pa)")
        axs[1].set_ylim(1000, 1500)

        for value in pres_points:
            axs[1].plot(value[0], value[1], 'ro')

        
        plt.show()





