#include <Arduino.h>
#include "consola_s/Surface.h"
void setup() { surface::begin(); }
void loop() { surface::poll(); }
