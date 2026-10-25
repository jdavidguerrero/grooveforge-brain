// Nombre del dispositivo USB (MIDI, audio y serie) que ven Live y macOS: "GrooveForge" en vez de "Teensy MIDI".
// Reemplaza los símbolos débiles usb_string_product_name / usb_string_manufacturer_name del core de Teensyduino.
#include "usb_names.h"

#define GF_PRODUCT_NAME {'G', 'r', 'o', 'o', 'v', 'e', 'F', 'o', 'r', 'g', 'e'}
#define GF_PRODUCT_NAME_LEN 11

struct usb_string_descriptor_struct usb_string_product_name = {
    2 + GF_PRODUCT_NAME_LEN * 2, 3, GF_PRODUCT_NAME};

struct usb_string_descriptor_struct usb_string_manufacturer_name = {
    2 + GF_PRODUCT_NAME_LEN * 2, 3, GF_PRODUCT_NAME};
