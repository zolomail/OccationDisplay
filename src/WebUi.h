#pragma once

#include <Arduino.h>
#include <memory>
#include "effects/ScrollText.h"

namespace WebUi {

void begin(std::shared_ptr<ScrollText> scroller);
void handleClient();
void startAccessPoint(const char* reason);

bool isApMode();
bool isTestMode();
bool showIp();

const String& wifiError();
const char*   apSsid();
const char*   apPassword();

} // namespace WebUi
