from struct import pack, unpack
from serial import Serial


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
        while len(self.window_data) < self.window_size:
            waiting = self.ser.in_waiting
            if waiting >= 8:
                new_buffer = self.ser.read(8)
                self.window_data.append(unpack('ff', new_buffer))
        waiting = self.ser.in_waiting
        tail_data_size = 48
        if waiting >= tail_data_size:  # 12 floats * 4 bytes
            new_buffer = self.ser.read(tail_data_size)
            print(new_buffer)
            values = unpack('<5f5fff', new_buffer)
            (self.temp_top, self.pres_top, self.temp_rms,
             self.pres_rms) = (
                 list(values[0:5]),
                 list(values[5:10]),
                 values[10],
                 values[11])
            # TODO: Actualizar visualización

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
