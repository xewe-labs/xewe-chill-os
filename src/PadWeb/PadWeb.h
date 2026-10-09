// SPDX-FileCopyrightText: 2025-2026 Maxim Dokukin (maxdokukin.com)
// SPDX-License-Identifier: LicenseRef-PolyForm-NC-1.0.0-NoAI
// xewe-os-laptop-cooling-pad/src/PadWeb/PadWeb.h
//
// The cooling pad's web page and JSON API, added to the web-interface module's server
// (web_interface.get_server()). Not a module: no CLI group, no NVS. The page lives at /pad because
// / belongs to the web-interface module (MIGRATION.md CP5).
#pragma once

#include <WebServer.h>

#include "../Fan/Fan.h"
#include "../Mlx90614/Mlx90614.h"
#include "../Argb/Argb.h"
#include "../FanCurve/FanCurve.h"


class PadWeb {
public:
                                PadWeb                      (Fan& fan_ref, Mlx90614& mlx90614_ref,
                                                             Argb& argb_ref, FanCurve& fan_curve_ref);

    // registers the routes; call once, before or after the server begins
    void                        attach                      (WebServer& server);

private:
    void                        send_json                   (const std::string& body, int code = 200);
    void                        handle_ui_config_post       ();

    WebServer*                  server                      {nullptr};
    Fan&                        fan;
    Mlx90614&                   mlx90614;
    Argb&                       argb;
    FanCurve&                   fan_curve;
};
