from t4_gui import Ui_MainWindow
import sys
from PyQt5 import QtWidgets
from controller import Controller
import serial
from dotenv import load_dotenv
import os

load_dotenv()

PORT = os.getenv('SERIAL_PORT')
BAUD_RATE = 115200


class MainWindow(QtWidgets.QMainWindow):
    def __init__(self, port: serial.Serial = None, ctrl: Controller = None):
        super().__init__()
        self.ui = Ui_MainWindow()
        self.serial_port = port
        self.controller = ctrl
        self.init_ui()

    def init_ui(self):
        self.ui.setupUi(self)
        self.ui.boton_configuracion_3.clicked.connect(self.change_window_size)
        self.ui.boton_inicio.clicked.connect(self.request_window)
        self.ui.boton_detener.clicked.connect(self.shutdown)

    def request_window(self):
        self.controller.request_window()
        self.controller.get_window()

    def change_window_size(self):
        self.controller.change_window_size(self.ui.spinBox.value())

    def shutdown(self):
        self.controller.shutdown()

    def closeEvent(self, event):
        """Restore stdout when closing the application."""
        sys.stdout = sys.__stdout__  # Restore original stdout
        sys.stderr = sys.__stderr__  # Restore original stderr if redirected
        self.controller.close()
        super().closeEvent(event)


if __name__ == '__main__':
    app = QtWidgets.QApplication(sys.argv)
    serial_port = serial.Serial(PORT, BAUD_RATE, timeout=1)
    controller = Controller(serial_port)
    window = MainWindow(serial_port, controller)
    window.show()
    sys.exit(app.exec())
