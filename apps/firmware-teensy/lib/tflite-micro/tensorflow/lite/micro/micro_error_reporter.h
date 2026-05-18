// Compatibility shim: micro_error_reporter.h fue movido a tflite_bridge/ en
// versiones recientes de TFLite Micro. Este header redirige al nuevo path.
// Los modelos ML del proyecto incluyen el path antiguo — este shim evita
// tener que modificar los .cpp de cada modelo.
#pragma once
#include "tensorflow/lite/micro/tflite_bridge/micro_error_reporter.h"
