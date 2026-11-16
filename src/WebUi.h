#pragma once

#include <Arduino.h>
#include "Scene.h"

namespace WebUi {

void begin();
void handleClient();
void startAccessPoint(const char* reason);

bool    isApMode();
SceneId selectedScene();
bool    showIp();
bool    consumeSceneDirty();

const String& scrollText();
const String& wifiError();
const char*   apSsid();
const char*   apPassword();

} // namespace WebUi
