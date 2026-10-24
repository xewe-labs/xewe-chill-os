// SPDX-FileCopyrightText: 2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: GPL-3.0-only
// xewe-chill-os/xewe-chill-os.ino

// The project's values, first so a plain Arduino IDE build reads them too.
#include "Config.h"

#include <XeWeCore.h>

// The Os, before every module: modules register with it from their constructors.
XeWeOs os({.url = "https://github.com/xewe-labs/xewe-chill-os"});

// src/Modules.h is generated.
#if __has_include("src/Modules.h")
#include "src/Modules.h"
#endif

#include "src/Fan/Fan.h"
Fan fan(os);

#include "src/Mlx90614/Mlx90614.h"
Mlx90614 mlx90614(os);

#include "src/PadWeb/PadWeb.h"
PadWeb pad_web(os, fan, mlx90614, led);

void setup() {
    pad_web.attach(web_interface);
    os.begin();
}

void loop() {
    os.loop();
}
