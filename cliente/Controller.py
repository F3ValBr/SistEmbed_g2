from struct import pack, unpack


class MissingWindowSizeError(Exception):
    pass


class Controller:
    running = True

    def __init__(self, serial_port=None):
        self.window_size = 10
        self.ser = serial_port

    def receive_response(self):
        response = self.ser.readline()
        return response

    def send_message(self, message):
        """Funcion para enviar un mensaje a la ESP32"""
        self.ser.write(message)

    def request_window(self):
        self.send_message(self.pack_signal(0))

    def change_window_size(self, new_window_size):
        try:
            new_window_size = int(new_window_size)
            self.send_message(self.pack_signal(1, new_window_size))
            print(f"El tamaño de la ventana se ha cambiado a {new_window_size}")
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
        return signal_type.to_bytes(1, 'little') + body.to_bytes(4, "little")
