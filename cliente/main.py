from Controller import Controller
import serial


PORT = '/dev/cu.usbserial-110'
BAUD_RATE = 115200

def main():
    serial_port = serial.Serial(PORT, BAUD_RATE, timeout=1)
    controller = Controller(serial_port)

    while True:
        # Recibe instrucción del usuario.
        print('Press 0 to receive data')
        print('Press 1 to change window size')
        print('Press 2 to end communication')
        user_input = input('Enter your choice: ')

        if user_input == '0':
            controller.request_window()
            controller.get_window()
        elif user_input == '1':
            new_window_size = input('Enter new window size: ')
            controller.change_window_size(new_window_size)
        elif user_input == '2':
            controller.shutdown()
            break
        else:
            print('Invalid input')
            continue

    controller.close()


if __name__ == "__main__":
    main()
