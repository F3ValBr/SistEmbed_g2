import pytest
import serial

from cliente.Controller import Controller
from cliente.Controller import MissingWindowSizeError

# on which port should the tests be performed:
PORT = 'loop://'


@pytest.fixture
def mock_serial():
    srl = serial.serial_for_url(PORT, timeout=1)
    yield srl
    srl.close()


@pytest.fixture
def mock_controller(mock_serial):
    ctrl = Controller(mock_serial)
    yield ctrl


def test_request_window(mock_controller):
    """Cliente debería enviar señal de iniciar ventana."""
    mock_controller.request_window()
    assert mock_controller.ser.read() == b'\x00', \
        'debió enviarse solicitud de ventana'


def test_change_window_size(mock_controller):
    """Cliente debería enviar señal de cambio de tamaño."""
    mock_controller.change_window_size(100)
    assert mock_controller.ser.read(5) == b'\x01\x64\x00\x00\x00', \
        'debió enviarse solicitud cambio de tamaño'


def test_shutdown(mock_controller):
    """Cliente debería enviar señal de cierre y aplicación debería terminar."""
    mock_controller.shutdown()
    assert mock_controller.ser.read() == b'\x02', \
        'debió enviarse señal de apagado'
    assert mock_controller.running is False, \
        'debió cambiar el estado de la aplicación a apagado'


def test_pack_signal(mock_controller):
    """Debe empaquetar según el tipo de señal y cuerpo."""
    assert mock_controller.pack_signal(0) == b'\x00', \
        'debió empaquetar un solo byte'
    assert mock_controller.pack_signal(2) == b'\x02', \
        'debió empaquetar un solo byte'
    assert mock_controller.pack_signal(1, 100) == b'\x01\x64\x00\x00\x00', \
        'debió empaquetar tipo de señal y cuerpo'
    with pytest.raises(MissingWindowSizeError):
        mock_controller.pack_signal(1)
