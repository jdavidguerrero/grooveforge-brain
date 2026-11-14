#pragma once

void setupHardware();
void loopHardware();
void setSelectedTrack(int trackIndex); // Update selected track for encoders
void handleMixerBankChangeFromGUI(int bank); // Handle mixer bank change from GUI
