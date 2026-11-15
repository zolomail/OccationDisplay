#pragma once

#include <Arduino.h>
#include <memory>
#include "Scene.h"
#include "effects/ScrollText.h"

namespace WebUi {

void begin(std::shared_ptr<ScrollText> scroller);
void handleClient();
void startAccessPoint(const char* reason);

bool    isApMode();
SceneId selectedScene();
bool    showIp();

const String& wifiError();
const char*   apSsid();
const char*   apPassword();

} // namespace WebUi
