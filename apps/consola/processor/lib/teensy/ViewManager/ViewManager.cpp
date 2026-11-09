#include "ViewManager.h"
#include <Arduino.h>

ViewManager::ViewManager()
    : currentView(ViewType::SESSION)
    , previousView(ViewType::SESSION)
    , onViewChanged(nullptr)
{
}

void ViewManager::begin() {
    Serial.println("ViewManager initialized");
    Serial.printf("  Current view: %s\n", getViewName(currentView));
}

void ViewManager::switchView(ViewType newView) {
    if (newView == currentView) {
        return;  // Ya estamos en esa vista
    }

    ViewType oldView = currentView;
    previousView = currentView;
    currentView = newView;

    Serial.printf("View changed: %s → %s\n",
                 getViewName(oldView),
                 getViewName(currentView));

    notifyViewChange(oldView);
}

void ViewManager::switchView(uint8_t viewIndex) {
    if (viewIndex > static_cast<uint8_t>(ViewType::BROWSE)) {
        Serial.printf("WARNING: Invalid view index %d\n", viewIndex);
        return;
    }

    switchView(static_cast<ViewType>(viewIndex));
}

void ViewManager::returnToPreviousView() {
    if (previousView != currentView) {
        Serial.printf("Returning to previous view: %s\n", getViewName(previousView));
        switchView(previousView);
    }
}

void ViewManager::notifyViewChange(ViewType oldView) {
    if (onViewChanged) {
        onViewChanged(currentView, oldView);
    }
}

const char* ViewManager::getViewName(ViewType view) {
    switch (view) {
        case ViewType::SESSION: return "SESSION";
        case ViewType::MIX:     return "MIX";
        case ViewType::DEVICE:  return "DEVICE";
        case ViewType::NOTE:    return "NOTE";
        case ViewType::BROWSE:  return "BROWSE";
        default:                return "UNKNOWN";
    }
}
