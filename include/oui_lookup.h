#pragma once
#include <Arduino.h>

String lookupOUI(const String& address);
String decodeManufacturerID(uint16_t mfrId);
String decodeAppleType(uint8_t type);
String decodeAppearance(uint16_t appearance);
String decodeServiceUUID(const String& uuid);