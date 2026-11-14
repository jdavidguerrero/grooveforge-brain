// Nombre USB propio de la Teensy S: con la Teensy A también conectada al Pi, las dos se llamarían «Teensy MIDI»
// y el lector (tools/consola/superficie.py) no podría distinguirlas. Mecanismo oficial de Teensyduino: el símbolo
// usb_string_product_name reemplaza al nombre por defecto (https://www.pjrc.com/teensy/td_midi.html, «Custom Names»).
#include "usb_names.h"

#define GF_NOMBRE {'G','F',' ','S','u','p','e','r','f','i','c','i','e'}
#define GF_NOMBRE_LEN 13

struct usb_string_descriptor_struct usb_string_product_name = {
    2 + GF_NOMBRE_LEN * 2,
    3,
    GF_NOMBRE
};
