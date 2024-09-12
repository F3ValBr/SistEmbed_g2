from struct import unpack
from serial import Serial
from time import sleep


class MissingWindowSizeError(Exception):
    pass


class Controller:
    running = True
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

    def get_window(self):
        # while self.ser.in_waiting < 4:
        #     pass
        # (self.window_size) = unpack('i', self.ser.read(4))
        # print(self.window_size)
        while len(self.window_data) < self.window_size:
            waiting = self.ser.in_waiting
            if waiting >= 8:
                sleep(1)
                new_buffer = self.ser.read(9)[:-1]
                # new_buffer = self.ser.readline()
                print(len(new_buffer))
                self.window_data.append(unpack('ff', new_buffer))
                print(self.window_size)
        # waiting = self.ser.in_waiting
        # tail_data_size = 48
        # if waiting >= tail_data_size:  # 12 floats * 4 bytes
        #     new_buffer = self.ser.read(tail_data_size)
        #     print(new_buffer)
        #     values = unpack('<5f5fff', new_buffer)
        #     (self.temp_top, self.pres_top, self.temp_rms,
        #      self.pres_rms) = (
        #          list(values[0:5]),
        #          list(values[5:10]),
        #          values[10],
        #          values[11])
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
