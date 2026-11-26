#pragma once

#include <Arduino.h>
#include "Scene.h"
#include "FastHub75.h"

namespace WebUi {

void begin();
void handleClient();
void reconnectStation();
void setWifiStatus(const char* status);

bool     isApReady();
uint32_t apUpMs();
bool     hasClientRequested();
bool     hasWifiCredentials();
SceneId  selectedScene();
bool     showIp();
bool     showCpuLoad();
uint16_t panelChain();
uint8_t  brightness();
const PanelCalibration* panelCalibrations();
bool     consumeSceneDirty();

const String& wifiError();
const char*   apSsid();
const char*   apPassword();

} // namespace WebUi
