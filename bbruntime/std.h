#ifndef STD_H
#define STD_H

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define POINTER_64 __ptr64

#include <Windows.h>

#include "constants.h"
#include "../config/config.h"
#include "../stdutil/stdutil.h"

#include <set>
#include <map>
#include <list>
#include <string>
#include <vector>
#include <iostream>
#include <fstream>

#include <cmath>

#define ErrorLog(function, log) if (debug) { RTEX(log) } else { errorfunc = function; errorlog = log; }

#endif