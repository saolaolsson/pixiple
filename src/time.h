#pragma once

#include <chrono>
#include <sstream>

using namespace std::chrono_literals;

std::wostream& operator<<(std::wostream& os, const std::chrono::system_clock::time_point tp);
std::wostream& operator<<(std::wostream& os, const std::chrono::system_clock::duration d);

