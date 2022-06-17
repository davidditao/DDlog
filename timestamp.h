#pragma once

#include <string>

using namespace std;

class Timestamp{
public:
    static int64_t now();
    static const int kMicroSecondsPerSecond = 1000 * 1000;    
};