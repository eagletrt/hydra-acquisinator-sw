"""Apertura robusta del transceiver gs_usb (candleLight / cannine style) per python-can.

All'avvio la libreria gs_usb resetta il device USB. Su macOS questo crea due finestre
di qualche decina di ms in cui l'apertura fallisce a caso:
  - il device si sta ri-enumerando: non viene trovato, oppure il reset da' "No such device";
  - subito dopo il reset libusb riporta per un attimo l'interfaccia come occupata da un
    driver kernel: la libreria prova a staccarlo, cosa che su macOS richiede root e
    fallisce con "Access denied (insufficient permissions)".
Qui il distacco viene saltato (su macOS non esiste un driver kernel per gs_usb) e
l'apertura viene ritentata finche' il device non e' di nuovo pronto.
"""

import gc
import sys
import time

import can
import usb.core
import usb.util

VENDOR_ID = 0x1D50
PRODUCT_ID = 0x606F
OPEN_TIMEOUT_S = 20.0
RETRY_DELAY_S = 0.5


def open_bus(bitrate: int) -> can.BusABC:
    """Apre il transceiver a `bitrate`; solleva CanInitializationError se non ci riesce."""
    if sys.platform == "darwin":
        usb.core.Device.is_kernel_driver_active = lambda self, interface: False

    deadline = time.monotonic() + OPEN_TIMEOUT_S
    last_error = "transceiver non trovato"
    while True:
        dev = usb.core.find(idVendor=VENDOR_ID, idProduct=PRODUCT_ID)
        if dev is not None:
            try:
                channel, usb_bus, address = dev.product, dev.bus, dev.address
                usb.util.dispose_resources(dev)  # la libreria riapre il device per conto suo
                return can.Bus(interface="gs_usb", channel=channel, bus=usb_bus, address=address, bitrate=bitrate)
            # ValueError: "The device has no langid", stringhe USB non ancora leggibili durante la ri-enumerazione
            except (usb.core.USBError, can.CanError, ValueError) as error:
                last_error = str(error)
            gc.collect()  # chiude subito un eventuale bus rimasto a meta', non all'uscita

        if time.monotonic() >= deadline:
            raise can.CanInitializationError(f"impossibile aprire il transceiver gs_usb: {last_error}")
        time.sleep(RETRY_DELAY_S)
