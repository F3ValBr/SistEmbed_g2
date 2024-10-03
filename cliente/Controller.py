from struct import unpack, pack
from serial import Serial
from time import sleep
import numpy as np
import matplotlib.pyplot as plt

# pylint: disable=broad-exception-caught, lost-exception


class MissingWindowSizeError(Exception):
    pass


class Controller:
    """Clase que agrupa métodos para el flujo de datos."""
    running = True
    window_data = []
    temp_top = None
    pres_top = None
    hum_top = None
    gas_top = None
    temp_rms = None
    pres_rms = None
    hum_rms = None
    gas_rms = None
    temp_fft = []
    pres_fft = []
    hum_fft = []
    gas_fft = []

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
        data = unpack("ffff", data)
        return data

    def receive_fft_pack(self):
        data = self.ser.read(32)
        data = unpack("8f", data)
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
        print("Extrayendo ventana...")
        window_received = False
        win_response = None
        while True:
            if self.ser.in_waiting > 7:
                try:
                    win_response = self.ser.read(8)
                    win_response = unpack("Q", win_response)[0]
                except Exception as e:
                    print(e)
                    continue
                else:
                    window_received = True
                    print(f"Tamano de ventana: {win_response}")
                    break
        if window_received:
            self.window_size = int(win_response)
        else:
            print("No se pudo obtener el tamano de la ventana")
            return

        print("Solicitando datos...")
        # generar sleep segun ventana obtenida para dar tiempo a la esp32
        sleep(self.window_size * 0.5)

        print("Recibiendo datos...")
        # enviar mensaje de inicio
        message = pack("6s", "BEGIN\0".encode())
        self.send_message(message)

        # recibir datos de la bme - mediciones
        counter = 0
        while True:
            if self.ser.in_waiting > 15:
                try:
                    obt_data = self.receive_bme_data()
                    print(obt_data)
                    # self.window_data.append(obt_data)
                    # Verifica si el primer elemento (temperatura) es válido
                    temperatura = obt_data[0]  # Primer elemento de la tupla
                    if (-50.0 <= temperatura <= 100.0 and
                            abs(temperatura) > 0.001):
                        self.window_data.append(obt_data)
                    else:
                        # Si no es válida, no se agrega y sigue esperando
                        print(f"Temperatura fuera de rango: {temperatura}")
                        counter -= 1
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

        # recibir datos del top 5
        print("Obteniendo medidas - Top 5")
        counter_measures = 0
        top5_array = []

        while True:
            if self.ser.in_waiting > 19:
                try:
                    top_data = self.receive_top()
                    print(top_data)
                    top5_array.append(top_data)
                except Exception as e:
                    print(e)
                    continue
                else:
                    counter_measures += 1
                    print(f"Contador data: {counter_measures}")
                finally:
                    if counter_measures == 4:
                        print("Medidas del top 5 obtenidas")
                        self.window_data.append(top5_array)
                        break

        print("Datos obtenidos")
        print(self.window_data)
        sleep(1)

        # recibir datos del rms
        print("Obteniendo medidas - RMS")
        counter_rms = 0

        while True:
            if self.ser.in_waiting > 15:
                try:
                    rms_data = self.receive_bme_data()
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
        counter_fft = 0
        self.temp_fft = []
        self.pres_fft = []
        self.hum_fft = []
        self.gas_fft = []
        while True:
            if self.ser.in_waiting > 31:
                try:
                    fft_data = self.receive_fft_pack()
                    self.window_data.append(fft_data)
                    self.temp_fft.append((fft_data[0], fft_data[1]))
                    self.pres_fft.append((fft_data[2], fft_data[3]))
                    self.hum_fft.append((fft_data[4], fft_data[5]))
                    self.gas_fft.append((fft_data[6], fft_data[7]))
                except Exception as e:
                    print(e)
                    continue
                else:
                    counter_fft += 1
                finally:
                    if counter_fft == self.window_size:
                        print("FFT obtenido")
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
            return signal_type.to_bytes(1, "little")
        return signal_type.to_bytes(1, "little") + body.to_bytes(4, "little")

    def plot_window(self):
        data_bme = self.window_data[0:self.window_size]
        data_top = self.window_data[-2]
        data_rms = self.window_data[-1]
        self.temp_top = data_top[0]
        self.pres_top = data_top[1]
        self.hum_top = data_top[2]
        self.gas_top = data_top[3]
        self.temp_rms = data_rms[0]
        self.pres_rms = data_rms[1]
        self.hum_rms = data_rms[2]
        self.gas_rms = data_rms[3]

        data_array = np.array(data_bme)
        data_temp = data_array[:, 0]
        data_pres = data_array[:, 1]
        data_hum = data_array[:, 2]
        data_gas = data_array[:, 3]

        temp_points = []
        pres_points = []
        hum_points = []
        gas_points = []

        for i in range(5):
            temp_idx = np.where(data_temp == self.temp_top[i])
            temp_points.append((temp_idx, self.temp_top[i]))

            pres_idx = np.where(data_pres == self.pres_top[i])
            pres_points.append((pres_idx, self.pres_top[i]))

            hum_idx = np.where(data_hum == self.hum_top[i])
            hum_points.append((hum_idx, self.hum_top[i]))

            gas_idx = np.where(data_gas == self.gas_top[i])
            gas_points.append((gas_idx, self.gas_top[i]))

        temp_fft_vals = [np.sqrt(x[0]**2 + x[1]**2) for x in self.temp_fft]
        pres_fft_vals = [np.sqrt(x[0]**2 + x[1]**2) for x in self.pres_fft]
        hum_fft_vals = [np.sqrt(x[0]**2 + x[1]**2) for x in self.hum_fft]
        gas_fft_vals = [np.sqrt(x[0]**2 + x[1]**2) for x in self.gas_fft]

        fig, axs = plt.subplots(2, 4)

        fig.suptitle(
            "Temperatura, Presion, Humedad y Gas" +
            f"(ventana: {self.window_size}")

        axs[0, 0].plot(data_temp)
        axs[0, 0].set_title(f"Temperatura (RMS = {self.temp_rms})")
        axs[0, 0].set_ylabel("Grados Celsius (°C)")
        axs[0, 0].set_ylim(0, 50)

        for value in temp_points:
            axs[0, 0].plot(value[0], value[1], "ro")

        axs[0, 1].plot(data_pres)
        axs[0, 1].set_title(f"Presion (RMS = {self.pres_rms})")
        axs[0, 1].set_ylabel("Pascales (Pa)")
        axs[0, 1].set_ylim(1000, 1500)

        for value in pres_points:
            axs[0, 1].plot(value[0], value[1], "ro")

        axs[0, 2].plot(data_hum)
        axs[0, 2].set_title(f"Humedad (RMS = {self.hum_rms})")
        axs[0, 2].set_ylabel("Porcentaje (%)")
        axs[0, 2].set_ylim(0, 100)

        for value in hum_points:
            axs[0, 2].plot(value[0], value[1], "ro")

        axs[0, 3].plot(data_gas)
        axs[0, 3].set_title(f"Gas (RMS = {self.gas_rms})")
        axs[0, 3].set_ylabel("Ohmios (Ω)")
        axs[0, 3].set_ylim(0, 100)

        for value in gas_points:
            axs[0, 3].plot(value[0], value[1], "ro")

        axs[1, 0].plot(temp_fft_vals)
        axs[1, 0].set_title("FFT Temperatura")
        axs[1, 0].set_ylabel("Amplitud")

        axs[1, 1].plot(pres_fft_vals)
        axs[1, 1].set_title("FFT Presion")
        axs[1, 1].set_ylabel("Amplitud")

        axs[1, 2].plot(hum_fft_vals)
        axs[1, 2].set_title("FFT Humedad")
        axs[1, 2].set_ylabel("Amplitud")

        axs[1, 3].plot(gas_fft_vals)
        axs[1, 3].set_title("FFT Gas")
        axs[1, 3].set_ylabel("Amplitud")

        plt.show()
