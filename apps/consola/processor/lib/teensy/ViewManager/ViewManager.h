#pragma once

#include "shared/Config.h"
#include <stdint.h>

// Tipos de vista disponibles
enum class ViewType : uint8_t {
    SESSION = 0,
    MIX = 1,
    DEVICE = 2,
    NOTE = 3,
    BROWSE = 4
};

class ViewManager {
public:
    ViewManager();

    void begin();

    // Cambiar vista
    void switchView(ViewType newView);
    void switchView(uint8_t viewIndex);

    // Estado actual
    ViewType getCurrentView() const { return currentView; }
    uint8_t getCurrentViewIndex() const { return static_cast<uint8_t>(currentView); }

    // Vista anterior (para volver atrás)
    ViewType getPreviousView() const { return previousView; }
    void returnToPreviousView();

    // Callbacks para notificar cambios
    void (*onViewChanged)(ViewType newView, ViewType oldView);

private:
    ViewType currentView;
    ViewType previousView;

    void notifyViewChange(ViewType oldView);
    const char* getViewName(ViewType view);
};
