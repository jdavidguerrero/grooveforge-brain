#include "usb_names.h"
#include "consola_s/ConsolaMap.h"
struct usb_string_descriptor_struct usb_string_product_name = {
    2 + (sizeof(consola::USB_PRODUCT_TEXT)-1) * 2, 3, GF_CONSOLA_PRODUCT_CHARS
};
