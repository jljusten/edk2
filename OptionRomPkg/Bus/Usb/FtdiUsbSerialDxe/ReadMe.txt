
=== FTDI USB SERIAL OVERVIEW ===

This driver enables the EfiSerialIoProtocol interface
for FTDI8U232AM based USB-to-Serial adapters.

=== STATUS ===

Serial Input: Functional on emulated hardware via QEMU and real hardware.
Serial Output: Functional on emulated hardware via QEMU and real hardware.

Operating Modes: CUrrently the user is unable to change the operating modes.
The supported operating mode is:
	Baudrate:     115200
	Parity:       None
	Flow Control: None
	Data Bits:    8
	Stop Bits:    1

=== COMPATIBILITY ===

Tested with:
OVMF using QEMU parameters: -usb -usbdevice serial::vc

An FTDI8U232AM based USB-To-Serial adapter and the UEFI Shell (installed on a MacBook Air)
using the SerialTest Application located in SerialTestPkg.

