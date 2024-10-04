def checksum(msg: bytes) -> int:
    """Calcula el checksum de un mensaje.

    Args:
        msg (bytes): Mensaje a calcular.

    Returns:
        int: Valor del checksum
    """
    sum_value = 0
    for b in msg:
        sum_value += b

    # sum_value += 4294967295
    return sum_value & 0xffffffff
