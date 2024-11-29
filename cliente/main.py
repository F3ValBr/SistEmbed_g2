from controller import Controller
import serial
from dotenv import load_dotenv
import os

load_dotenv()


PORT = os.getenv("SERIAL_PORT")
BAUD_RATE = 115200


def routine_controller(signal, controller):
    if signal == "0":
        controller.request_window()
        controller.get_window()
    if signal == "1":
        new_window_size = input("Enter new window size: ")
        controller.change_window_size(new_window_size)
    if signal == "2":
        controller.shutdown()
        return False
    else:
        print("Invalid input")

    return True


def main():
    serial_port = serial.Serial(PORT, BAUD_RATE, timeout=1)
    controller = Controller(serial_port)

    while True:
        # Recibe instrucción del usuario.
        print("Press 0 to receive data")
        print("Press 1 to change window size")
        print("Press 2 to end communication")
        user_input = input("Enter your choice: ")

        code = routine_controller(user_input, controller)

        if code:
            continue
        else:
            break

    controller.close()


if __name__ == "__main__":
    main()
