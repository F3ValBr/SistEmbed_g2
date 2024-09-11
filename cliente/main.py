from Controller import Controller
import serial


PORT = '/dev/ttyUSB0'
BAUD_RATE = 115200

"""
def main():
    print("Opciones:")
    print("0. Solicitar una ventana de datos")
    print("1. Cambiar tamaño de ventana")
    print("2. Cerrar la conexión")
    option = input("Elige una opción: ")

    if option == '1':
        #   algo
        pass
    elif option == '2':
        n_ventana = int(input("Ingrese el valor de la nueva ventana: "))

    elif option == '3':
        print("Cerrando la conexión...")
    else:
        print("Opción no válida.")
        main()
"""


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
            # TODO: Leer datos enviados.
            # TODO: visualize data
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
