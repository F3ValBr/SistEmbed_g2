from struct import unpack, pack, calcsize
from serial import Serial
from time import sleep
import numpy as np
import matplotlib.pyplot as plt

# pylint: disable=broad-exception-caught, lost-exception


class MissingWindowSizeError(Exception):
    pass


READING_VALUE_QTY = 6


class Controller:
    """Clase que agrupa métodos para el flujo de datos."""
    running = True
    window_data = []
    # TODO: reemplazar campos de gyro + acc
    acc_x_top = None
    acc_y_top = None
    acc_z_top = None
    gyro_x_top = None
    gyro_y_top = None
    gyro_z_top = None
    acc_x_rms = None
    acc_y_rms = None
    acc_z_rms = None
    gyro_x_rms = None
    gyro_y_rms = None
    gyro_z_rms = None
    acc_x_fft = []
    acc_y_fft = []
    acc_z_fft = []
    gyro_x_fft = []
    gyro_y_fft = []
    gyro_z_fft = []

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

    def receive_bmi_data(self):
        data = self.ser.read(READING_VALUE_QTY * calcsize("f"))
        data = unpack(f"{READING_VALUE_QTY}f", data)
        return data

    def receive_fft_pack(self):
        data = self.ser.read(READING_VALUE_QTY * 2 * calcsize("f"))
        data = unpack(f"{READING_VALUE_QTY*2}f", data)
        return data

    def receive_top(self):
        data = self.ser.read(20)
        data = unpack("5f", data)
        return data

    # TODO: remover sin usar
    # def receive_rms(self):
    #     data = self.ser.read(12)
    #     data = unpack("fff", data)
    #     return data

    def get_window(self):
        print("Extrayendo ventana...")
        window_received = False
        win_response = None
        while True:
            if self.ser.in_waiting > calcsize('Q') - 1:
                try:
                    win_response = self.ser.read(calcsize('Q'))
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

        # recibir datos de la bmi - mediciones
        counter = 0
        while True:
            if self.ser.in_waiting > READING_VALUE_QTY * 4 - 1:
                try:
                    obt_data = self.receive_bmi_data()
                    print(obt_data)
                    self.window_data.append(obt_data)
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
            if self.ser.in_waiting > calcsize("f") * 5 - 1:
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
                    if counter_measures == READING_VALUE_QTY:
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
            if self.ser.in_waiting > READING_VALUE_QTY * calcsize("f") - 1:
                try:
                    rms_data = self.receive_bmi_data()
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
        # TODO: reemplazar arreglos de fft por gyro y acc
        self.acc_x_fft = []
        self.acc_y_fft = []
        self.acc_z_fft = []
        self.gyro_x_fft = []
        self.gyro_y_fft = []
        self.gyro_z_fft = []
        while True:
            if self.ser.in_waiting > READING_VALUE_QTY * 2 * calcsize("f") - 1:
                try:
                    fft_data = self.receive_fft_pack()
                    # self.window_data.append(fft_data)
                    self.acc_x_fft.append((fft_data[0], fft_data[1]))
                    self.acc_y_fft.append((fft_data[2], fft_data[3]))
                    self.acc_z_fft.append((fft_data[4], fft_data[5]))
                    self.gyro_x_fft.append((fft_data[6], fft_data[7]))
                    self.gyro_y_fft.append((fft_data[8], fft_data[9]))
                    self.gyro_z_fft.append((fft_data[10], fft_data[11]))
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
        data_bmi = self.window_data[0:self.window_size]
        data_top = self.window_data[-2]
        data_rms = self.window_data[-1]

        self.acc_x_top = data_top[0]
        self.acc_y_top = data_top[1]
        self.acc_z_top = data_top[2]
        self.gyro_x_top = data_top[3]
        self.gyro_y_top = data_top[4]
        self.gyro_z_top = data_top[5]

        self.acc_x_rms = data_rms[0]
        self.acc_y_rms = data_rms[1]
        self.acc_z_rms = data_rms[2]
        self.gyro_x_rms = data_rms[3]
        self.gyro_y_rms = data_rms[4]
        self.gyro_z_rms = data_rms[5]

        data_array = np.array(data_bmi)
        data_acc_x = data_array[:, 0]
        data_acc_y = data_array[:, 1]
        data_acc_z = data_array[:, 2]
        data_gyro_x = data_array[:, 3]
        data_gyro_y = data_array[:, 4]
        data_gyro_z = data_array[:, 5]

        acc_x_points = []
        acc_y_points = []
        acc_z_points = []
        gyro_x_points = []
        gyro_y_points = []
        gyro_z_points = []

        for i in range(5):
            acc_x_idx = np.where(data_acc_x == self.acc_x_top[i])
            acc_x_points.append((acc_x_idx, self.acc_x_top[i]))

            acc_y_idx = np.where(data_acc_y == self.acc_y_top[i])
            acc_y_points.append((acc_y_idx, self.acc_y_top[i]))

            acc_z_idx = np.where(data_acc_z == self.acc_z_top[i])
            acc_z_points.append((acc_z_idx, self.acc_z_top[i]))

            gyro_x_idx = np.where(data_gyro_x == self.gyro_x_top[i])
            gyro_x_points.append((gyro_x_idx, self.gyro_x_top[i]))

            gyro_y_idx = np.where(data_gyro_y == self.gyro_y_top[i])
            gyro_y_points.append((gyro_y_idx, self.gyro_y_top[i]))

            gyro_z_idx = np.where(data_gyro_z == self.gyro_z_top[i])
            gyro_z_points.append((gyro_z_idx, self.gyro_z_top[i]))

        acc_x_fft_vals = [np.sqrt(x[0]**2 + x[1]**2) for x in self.acc_x_fft]
        acc_y_fft_vals = [np.sqrt(x[0]**2 + x[1]**2) for x in self.acc_y_fft]
        acc_z_fft_vals = [np.sqrt(x[0]**2 + x[1]**2) for x in self.acc_z_fft]
        gyro_x_fft_vals = [np.sqrt(x[0]**2 + x[1]**2) for x in self.gyro_x_fft]
        gyro_y_fft_vals = [np.sqrt(x[0]**2 + x[1]**2) for x in self.gyro_y_fft]
        gyro_z_fft_vals = [np.sqrt(x[0]**2 + x[1]**2) for x in self.gyro_z_fft]

        fig, axs = plt.subplots(2, READING_VALUE_QTY)

        fig.suptitle(
            "Coordenadas Giroscopio y Acelerómetro" +
            f"(ventana: {self.window_size}")

        # TODO: verificar limites de valores
        axs[0, 0].plot(data_acc_x)
        axs[0, 0].set_title(
            f"Coord x - Acelerómetro \n(RMS = {self.acc_x_rms})")
        axs[0, 0].set_ylabel("x")
        # axs[0, 0].set_ylim(0, 50)

        for value in acc_x_points:
            axs[0, 0].plot(value[0], value[1], "ro")

        axs[0, 1].plot(data_acc_y)
        axs[0, 1].set_title(
            f"Coord y - Acelerómetro \n(RMS = {self.acc_y_rms})")
        axs[0, 1].set_ylabel("y")
        # axs[0, 1].set_ylim(1000, 1500)

        for value in acc_y_points:
            axs[0, 1].plot(value[0], value[1], "ro")

        axs[0, 2].plot(data_acc_z)
        axs[0, 2].set_title(
            f"Coord z - Acelerómetro \n(RMS = {self.acc_z_rms})")
        axs[0, 2].set_ylabel("z")
        # axs[0, 2].set_ylim(0, 100)

        for value in acc_z_points:
            axs[0, 2].plot(value[0], value[1], "ro")

        axs[0, 3].plot(data_gyro_x)
        axs[0, 3].set_title(
            f"Coord x - Giroscópio \n(RMS = {self.gyro_x_rms})")
        axs[0, 3].set_ylabel("x")
        # axs[0, 3].set_ylim(0, 100)

        for value in gyro_x_points:
            axs[0, 3].plot(value[0], value[1], "ro")

        axs[0, 4].plot(data_gyro_y)
        axs[0, 4].set_title(
            f"Coord y - Giroscópio \n(RMS = {self.gyro_y_rms})")
        axs[0, 4].set_ylabel("y")
        # axs[0, 3].set_ylim(0, 100)

        for value in gyro_y_points:
            axs[0, 4].plot(value[0], value[1], "ro")

        axs[0, 5].plot(data_gyro_z)
        axs[0, 5].set_title(
            f"Coord z - Giroscópio \n(RMS = {self.gyro_z_rms})")
        axs[0, 5].set_ylabel("x")
        # axs[0, 3].set_ylim(0, 100)

        for value in gyro_z_points:
            axs[0, 5].plot(value[0], value[1], "ro")

        axs[1, 0].plot(acc_x_fft_vals)
        axs[1, 0].set_title("FFT Coord x - Acelerómetro")
        axs[1, 0].set_ylabel("Amplitud")

        axs[1, 1].plot(acc_y_fft_vals)
        axs[1, 1].set_title("FFT Coord y - Acelerómetro")
        axs[1, 1].set_ylabel("Amplitud")

        axs[1, 2].plot(acc_z_fft_vals)
        axs[1, 2].set_title("FFT Coord z - Acelerómetro")
        axs[1, 2].set_ylabel("Amplitud")

        axs[1, 3].plot(gyro_x_fft_vals)
        axs[1, 3].set_title("FFT Coord x - Giroscópio")
        axs[1, 3].set_ylabel("Amplitud")

        axs[1, 4].plot(gyro_y_fft_vals)
        axs[1, 4].set_title("FFT Coord y - Giroscópio")
        axs[1, 4].set_ylabel("Amplitud")

        axs[1, 5].plot(gyro_z_fft_vals)
        axs[1, 5].set_title("FFT Coord z - Giroscópio")
        axs[1, 5].set_ylabel("Amplitud")

        plt.show()
