#pragma once
#include <Arduino.h>

extern bool sdActive;

void sdBegin();
void sdEnd();
void sdUpdate();

void sdPrintStatus();