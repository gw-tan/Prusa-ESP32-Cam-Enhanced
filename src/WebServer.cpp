/**
   @file server.cpp

   @brief Library for WEB server

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug

*/

#include "WebServer.h"
#include "WebPage_gz.h"
#include "Certificate.h"
#include "timelapse.h"
#include "prusa_link.h"
#include "ota.h"

#include <vector>
#include <algorithm>
#include <time.h>

AsyncWebServer server(WEB_SERVER_PORT);

/**
   @brief Load configuration from EEPROM
   @param none
   @return none
*/
void Server_LoadCfg() {
  WebBasicAuth.EnableAuth = SystemConfig.LoadBasicAuthFlag();
  WebBasicAuth.UserName = SystemConfig.LoadBasicAuthUsername();
  WebBasicAuth.Password = SystemConfig.LoadBasicAuthPassword();
}

/**
   @brief Init WEB server
   @param none
   @return none
*/
void Server_InitWebServer() {
  SystemLog.AddEvent(LogLevel_Info, F("Starting init WEB server"));

  /* route for get last capture photo */
  server.on("/saved-photo.jpg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: get photo"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (SystemCamera.GetCameraCaptureSuccess() == false) {
      request->send(404, "text/plain", "Photo not found!");
      return;
    }
    /* SetPhotoSending(true) prevents CapturePhoto() from overwriting FrameBuffer
       while the chunked response is in flight. PhotoSending is volatile. */
    SystemCamera.SetPhotoSending(true);

    SystemLog.AddEvent(LogLevel_Verbose, "Photo size: " + String(SystemCamera.GetPhotoFb()->len) + " bytes");

    if (SystemCamera.GetPhotoExifData()->header != NULL) {
      /* send photo with exif data — PhotoSending cleared in lambda after last chunk */
      SystemLog.AddEvent(LogLevel_Verbose, F("Send photo with EXIF data"));
      size_t total_len = SystemCamera.GetPhotoExifData()->len + SystemCamera.GetPhotoFb()->len - SystemCamera.GetPhotoExifData()->offset;
      auto response = request->beginChunkedResponse("image/jpg", [total_len](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        if (index >= total_len) {
          SystemCamera.SetPhotoSending(false);
          return 0;
        }
        size_t len = 0;
        if (index < SystemCamera.GetPhotoExifData()->len) {
          len = min(maxLen, SystemCamera.GetPhotoExifData()->len - index);
          memcpy(buffer, SystemCamera.GetPhotoExifData()->header + index, len);
        } else {
          size_t offset = index - SystemCamera.GetPhotoExifData()->len + SystemCamera.GetPhotoExifData()->offset;
          len = min(maxLen, SystemCamera.GetPhotoFb()->len - offset);
          memcpy(buffer, SystemCamera.GetPhotoFb()->buf + offset, len);
        }
        if (index + len >= total_len) {
          SystemCamera.SetPhotoSending(false);
        }
        return len;
      });
      response->addHeader("Content-Length", String(total_len));
      request->send(response);

    } else {
      /* send photo without exif data — PhotoSending cleared in lambda after last chunk */
      SystemLog.AddEvent(LogLevel_Verbose, F("Send photo without EXIF data"));
      size_t fb_len = SystemCamera.GetPhotoFb()->len;
      auto response = request->beginChunkedResponse("image/jpg", [fb_len](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
        if (index >= fb_len) {
          SystemCamera.SetPhotoSending(false);
          return 0;
        }
        size_t remaining = fb_len - index;
        size_t toSend = (maxLen < remaining) ? maxLen : remaining;
        memcpy(buffer, SystemCamera.GetPhotoFb()->buf + index, toSend);
        if (index + toSend >= fb_len) {
          SystemCamera.SetPhotoSending(false);
        }
        return toSend;
      });
      response->addHeader("Content-Length", String(fb_len));
      request->send(response);
    }
  });

  Server_InitWebServer_JsonData();
  Server_InitWebServer_WebPages();
  Server_InitWebServer_Icons();
  Server_InitWebServer_Actions();
  Server_InitWebServer_Sets();
  Server_InitWebServer_Stream();
  Server_InitWebServer_Timelapse();
  Server_InitWebServer_PrusaLink();

  /* GET /log — last 30 log entries as JSON array */
  server.on("/log", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (Server_CheckBasicAuth(request) == false) return;
    request->send(200, "application/json", SystemLog.GetRecentLogs());
  });

  /* GET /debug — runtime diagnostics for issue confirmation */
  server.on("/debug", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (Server_CheckBasicAuth(request) == false) return;
    JsonDocument doc;
    /* upload stats */
    doc["last_fb_len"]           = Connect.GetDbgLastFbLen();
    doc["last_content_len"]      = Connect.GetDbgLastContentLen();
    doc["last_sent_bytes"]       = Connect.GetDbgLastSentBytes();
    doc["last_fb_len_mod_frag"]  = Connect.GetDbgLastFbLen() % PHOTO_FRAGMENT_SIZE;
    doc["fragment_size"]         = PHOTO_FRAGMENT_SIZE;
    doc["last_upload_ok"]        = Connect.GetDbgLastUploadOk();
    /* camera state */
    doc["stream_active"]         = SystemCamera.GetStreamStatus();
    doc["photo_sending"]         = SystemCamera.GetPhotoSending();
    /* memory */
    doc["free_heap"]             = ESP.getFreeHeap();
    doc["min_free_heap"]         = ESP.getMinFreeHeap();
    doc["free_psram"]            = ESP.getFreePsram();
    doc["min_free_psram"]        = ESP.getMinFreePsram();
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  /* route for not found page */
  server.onNotFound(Server_handleNotFound);

  /* start WEB server */
  server.begin();
}

/**
   @brief Init WEB server json data
   @param none
   @return none
*/
void Server_InitWebServer_JsonData() {
  /* route for json with cfg parameters */
  server.on("/json_input", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: get json_input"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/plain"), Server_GetJsonData().c_str());
  });

  /* route for json with wifi networks */
  server.on("/json_wifi", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: get json_wifi"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/plain"), SystemWifiMngt.GetAvailableWifiNetworks().c_str());
  });

  /* route for san wi-fi networks */
  server.on("/wifi_scan", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: scan WI-FI networks"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_SCANNING);
    SystemWifiMngt.ScanWiFiNetwork();
  });

}

/**
   @brief Init WEB server web pages
   @param none
   @return none
*/
void Server_InitWebServer_WebPages() {
  /* Route for root / web page */
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get index.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleGzipRequest(request, "text/html", index_html_gz, index_html_gz_len);
  });

  /* Route for the stylesheet. Built from webpage/tailwind.src.css, not fetched from
     a CDN: the setup AP has no internet, so a remote stylesheet would leave the
     first-run WiFi page unstyled. */
  server.on("/styles.css", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get styles.css"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleGzipRequest(request, "text/css", styles_css_gz, styles_css_gz_len);
  });

  /* route to license page */
  server.on("/license.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get license.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", license_html);
  });

  /* route to privacy policy page */
  server.on("/privacypolicy.html", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get privacypolicy.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "text/html", privacypolicy_html);
  });

  /* route to logs page */
  server.on("/get_logs", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get get_logs.html"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (true == SystemLog.GetCardDetectedStatus()) {
      request->send(SD_MMC, SystemLog.GetFilePath() + SystemLog.GetFileName(), "text/plain");
      //SystemLog.LogOpenFile();
    } else {
      request->send(404, "text/plain", "Micro SD card not found with FAT32 partition!");
    }
  });

}

/**
   @brief Init WEB server icons

   The SPA draws its own icons inline, so the favicon is the only one it ever
   fetches. The upstream icon set was dropped along with the multi-page UI it
   belonged to.

   @param none
   @return none
*/
void Server_InitWebServer_Icons() {
  /* route to favicon */
  server.on("/favicon.svg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: Get favicon.svg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    Server_handleCacheRequest(request, "image/svg+xml", favicon_svg);
  });
}

/**
   @brief Init WEB server actions
   @param none
   @return none
*/
void Server_InitWebServer_Actions() {
  /*route for capture photo */
  server.on("/action_capture", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_capture Take photo"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    CaptureCommand_t cmd = CMD_CAPTURE_ONLY;
    xQueueSend(g_captureQueue, &cmd, 0);
    request->send(200, "text/plain", "Take Photo");
  });

  /* route for send photo to prusa backend */
  server.on("/action_send", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_send send photo to cloud"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    CaptureCommand_t cmd = CMD_CAPTURE_AND_SEND;
    xQueueSend(g_captureQueue, &cmd, 0);
    request->send(200, "text/plain", "Send Photo");
  });

  /* route for change LED status */
  server.on("/action_led", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_led Change LED status"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    SystemCamera.SetFlashStatus(!SystemCamera.GetFlashStatus());
    SystemCamera.SetCameraFlashEnable(false);

    request->send(200, "text/plain", "Change LED status");
  });

  /* route for change LED status */
  server.on("/light", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /light set LED status"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (request->hasArg("on")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Turning light ON"));
      SystemCamera.SetFlashStatus(true);
      SystemCamera.SetCameraFlashEnable(false);
      request->send(200, "text/plain", "Light ON");

    } else if (request->hasArg("off")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Turning light OFF"));
      SystemCamera.SetFlashStatus(false);
      SystemCamera.SetCameraFlashEnable(false);
      request->send(200, "text/plain", "Light OFF");

    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });

  /* route for change FLASH status */
  server.on("/flash", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /flash set flash status"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (request->hasArg("on")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Turning flash ON"));
      SystemCamera.SetCameraFlashEnable(true);
      SystemCamera.SetFlashStatus(false);
      request->send(200, "text/plain", "Flash ON");

    } else if (request->hasArg("off")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Turning flash OFF"));
      SystemCamera.SetCameraFlashEnable(false);
      SystemCamera.SetFlashStatus(false);
      request->send(200, "text/plain", "Flash OFF");

    } else {
      request->send(400, "text/plain", "Invalid request");
    }
  });

  /* reboot MCU */
  server.on("/action_reboot", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /action_reboo reboot MCU!"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_REBOOT_MCU);
    /* delay() blocks TCP stack from flushing the response before restart;
       spawn a minimal task that yields for 500 ms then restarts */
    xTaskCreate([](void*) { vTaskDelay(pdMS_TO_TICKS(500)); ESP.restart(); vTaskDelete(NULL); },
                "reboot", 1024, NULL, 1, NULL);
  });

  /* /action_sderase — backing task was removed; return 501 so callers fail loudly */
  server.on("/action_sderase", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (Server_CheckBasicAuth(request) == false) return;
    request->send(501, F("text/plain"), "Not implemented");
  });
}

/**
   @brief Init WEB server sets
   @param none
   @return none
*/
void Server_InitWebServer_Sets() {
  /* route to set integer value */
  server.on("/set_int", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_int"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    bool response = false;
    String response_msg = "";

    /* set refresh interval */
    if (request->hasParam("refresh")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set refresh interval"));
      uint8_t value = request->getParam("refresh")->value().toInt();
      if ((value >= REFRESH_INTERVAL_MIN) && (value <= REFRESH_INTERVAL_MAX)) {
        Connect.SetRefreshInterval(value);
        response_msg = MSG_SAVE_OK;
      } else {
        response_msg = "ERROR! Bad value. Minimum is " + String(REFRESH_INTERVAL_MIN) + ", maximum " + String(REFRESH_INTERVAL_MAX) + " second";
      }
      response = true;
    }

    /* set saturation */
    if (request->hasParam("saturation")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set saturation"));
      SystemCamera.SetSaturation(request->getParam("saturation")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set contrast */
    if (request->hasParam("contrast")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set contrast"));
      SystemCamera.SetContrast(request->getParam("contrast")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set brightness */
    if (request->hasParam("brightness")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set brightness"));
      SystemCamera.SetBrightness(request->getParam("brightness")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set frame size */
    if (request->hasParam("framesize")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set framesize"));
      SystemCamera.SetFrameSize(request->getParam("framesize")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set photo quality */
    if (request->hasParam("photo_quality")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set photo_quality"));
      SystemCamera.SetPhotoQuality(73 - request->getParam("photo_quality")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set flash time */
    if (request->hasParam("flash_time")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set flash_time"));
      SystemCamera.SetCameraFlashTime(request->getParam("flash_time")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set white balancing mode */
    if (request->hasParam("wb_mode")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set wb_mode"));
      SystemCamera.SetAwbMode(request->getParam("wb_mode")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set auto exposition level */
    if (request->hasParam("ae_level")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set ae_level"));
      SystemCamera.SetAeLevel(request->getParam("ae_level")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set auto exposition controll value */
    if (request->hasParam("aec_value")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set aec_value"));
      SystemCamera.SetAecValue(request->getParam("aec_value")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set auto gain correction value */
    if (request->hasParam("agc_gain")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set agc_gain"));
      SystemCamera.SetAgcGain(request->getParam("agc_gain")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set image exif rotation */
    if (request->hasParam("image_rotation")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set image EXIF rotation"));
      SystemCamera.SetCameraImageRotation(request->getParam("image_rotation")->value().toInt());
      response_msg = MSG_SAVE_OK;
      response = true;
    }

    /* set log level /set_int?log_level=2 */
    if (request->hasParam("log_level")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set log_level"));
      LogLevel_enum level = (LogLevel_enum)request->getParam("log_level")->value().toInt();
      if ((level >= LogLevel_Error) && (level <= LogLevel_Verbose)) {
        SystemConfig.SaveLogLevel(level);
        SystemLog.SetLogLevel(level);
        response_msg = MSG_SAVE_OK;
      } else {
        response_msg = MSG_SAVE_NOTOK;
      }

      response = true;
    }

    /*  set network ip method. 0 - DHCP, 1 - Static */
    if (request->hasParam("ipcfg")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set ipcfg"));
      SystemWifiMngt.SetNetIpMethod(request->getParam("ipcfg")->value().toInt());
      response_msg = MSG_SAVE_OK_REBOOT;

      response = true;
    }

    if (true == response) {
      request->send(200, F("text/html"), response_msg.c_str());
    }
  });

  /* route to set bool value */
  server.on("/set_bool", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_bool"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    bool response = false;

    /* check cfg for hmirror */
    if (request->hasParam("hmirror")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set hmirror"));
      SystemCamera.SetHMirror(Server_TransfeStringToBool(request->getParam("hmirror")->value()));
      response = true;
    }

    /* set vertical flip */
    if (request->hasParam("vflip")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set hmirror"));
      SystemCamera.SetVFlip(Server_TransfeStringToBool(request->getParam("vflip")->value()));
      response = true;
    }

    /* set lens correction */
    if (request->hasParam("lenc")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set lensc"));
      SystemCamera.SetLensC(Server_TransfeStringToBool(request->getParam("lenc")->value()));
      response = true;
    }

    /* set exposure controll */
    if (request->hasParam("exposure_ctrl")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set exposure ctrl"));
      SystemCamera.SetExposureCtrl(Server_TransfeStringToBool(request->getParam("exposure_ctrl")->value()));
      response = true;
    }

    /* set auto white balancing */
    if (request->hasParam("awb")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set awb"));
      SystemCamera.SetAwb(Server_TransfeStringToBool(request->getParam("awb")->value()));
      response = true;
    }

    /* set auto white balancing gain */
    if (request->hasParam("awb_gain")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set awb_gain"));
      SystemCamera.SetAwbGain(Server_TransfeStringToBool(request->getParam("awb_gain")->value()));
      response = true;
    }

    /* set bad pixel correction */
    if (request->hasParam("bpc")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set bpc"));
      SystemCamera.SetBpc(Server_TransfeStringToBool(request->getParam("bpc")->value()));
      response = true;
    }

    /* set white pixel correction */
    if (request->hasParam("wpc")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set wpc"));
      SystemCamera.SetWpc(Server_TransfeStringToBool(request->getParam("wpc")->value()));
      response = true;
    }

    /* set raw gama correction */
    if (request->hasParam("raw_gama")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set raw_gama"));
      SystemCamera.SetRawGama(Server_TransfeStringToBool(request->getParam("raw_gama")->value()));
      response = true;
    }

    /* set automatic exposure correction */
    if (request->hasParam("aec2")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set aec2"));
      SystemCamera.SetAec2(Server_TransfeStringToBool(request->getParam("aec2")->value()));
      response = true;
    }

    /* set gain controll */
    if (request->hasParam("gain_ctrl")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set gain_ctrl"));
      SystemCamera.SetGainCtrl(Server_TransfeStringToBool(request->getParam("gain_ctrl")->value()));
      response = true;
    }

    /* set flash */
    if (request->hasParam("flash")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set flash"));
      SystemCamera.SetCameraFlashEnable(Server_TransfeStringToBool(request->getParam("flash")->value()));
      SystemCamera.SetFlashStatus(false);
      response = true;
    }

    /* set service AP */
    if (request->hasParam("serviceap_enable")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set service AP enable"));
      SystemWifiMngt.SetEnableServiceAp(Server_TransfeStringToBool(request->getParam("serviceap_enable")->value()));
      response = true;
    }

    /* set timelaps enable */
    if (request->hasParam("timelaps_enable")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set timelaps enable"));
#if (ENABLE_SD_CARD == true)
      bool val = Server_TransfeStringToBool(request->getParam("timelaps_enable")->value());
      if ((true == val) && (SystemLog.GetCardDetectedStatus() == true)) {
        Connect.SetTimeLapsPhotoSaveStatus(val);
      } else {
        Connect.SetTimeLapsPhotoSaveStatus(false);
      }
#else
        Connect.SetTimeLapsPhotoSaveStatus(false);
#endif
      response = true;
    }

    /* enable / disable upload to Prusa Connect */
    if (request->hasParam("upload_enable")) {
      SystemLog.AddEvent(LogLevel_Verbose, F("Set upload_enable"));
      Connect.SetUploadEnabled(Server_TransfeStringToBool(request->getParam("upload_enable")->value()));
      response = true;
    }

    if (true == response) {
      request->send(200, F("text/html"), MSG_SAVE_OK);
    }
  });

  /* route for set token for authentification to prusa backend*/
  server.on("/set_token", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_token"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_SAVE_OK);

    if (request->hasParam("token")) {
      /* enforce EEPROM field length before saving */
      String tok = request->getParam("token")->value();
      if (tok.length() < EEPROM_ADDR_TOKEN_LENGTH) {
        Connect.SetToken(tok);
      } else {
        SystemLog.AddEvent(LogLevel_Warning, "Rejected oversized token (" + String(tok.length()) + " bytes)");
      }
    }
  });

  /* route for set prusa connect hostname /set_hostname?hostname=*/
  server.on("/set_hostname", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_hostname"));
    if (Server_CheckBasicAuth(request) == false)
      return;
    request->send(200, F("text/html"), MSG_SAVE_OK);

    if (request->hasParam("hostname")) {
      Connect.SetPrusaConnectHostname(request->getParam("hostname")->value());
    }
  });

  /* route for set WI-FI credentials */
  server.on("/wifi_cfg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: set WI-FI credentials"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    String TmpPassword = "";
    String TmpSsid = "";
    /* get SSID */
    if (request->hasParam("wifi_ssid")) {
      TmpSsid = request->getParam("wifi_ssid")->value();
    }

    /* get password */
    if (request->hasParam("wifi_pass")) {
      TmpPassword = request->getParam("wifi_pass")->value();
    }

    /* check min and max length WI-FI ssid and password */
#if (WIFI_DISABLE_UNENCRYPTED_STA_PASS_CHECK == false)
    if (((TmpPassword.length() > 0) && (TmpSsid.length() > 0)) && ((TmpPassword.length() < EEPROM_ADDR_WIFI_PASSWORD_LENGTH) && (TmpSsid.length() < EEPROM_ADDR_WIFI_SSID_LENGTH))) {
#else
    if ((TmpSsid.length() > 0) && (TmpSsid.length() < EEPROM_ADDR_WIFI_SSID_LENGTH)) {
#endif

      /* send OK response */
      request->send(200, F("text/html"), MSG_SAVE_OK_WIFI);

      /* save ssid and password */
      SystemWifiMngt.SetStaCredentials(TmpSsid, TmpPassword);
      SystemWifiMngt.WiFiStaConnect();

    } else {
      request->send(200, F("text/html"), MSG_SAVE_NOTOK);
    }
  });

  /* route for set WI-FI static IP address */
  server.on("/wifi_net_cfg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: set WI-FI static IP address"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    String tmpIp = "";
    String tmpMask = "";
    String tmpGw = "";
    String tmpDns = "";

    /* get ip */
    if (request->hasParam("ip")) {
      tmpIp = request->getParam("ip")->value();
    }

    /* get mask */
    if (request->hasParam("mask")) {
      tmpMask = request->getParam("mask")->value();
    }

    /* get gw */
    if (request->hasParam("gw")) {
      tmpGw = request->getParam("gw")->value();
    }

    /* get dns */
    if (request->hasParam("dns")) {
      tmpDns = request->getParam("dns")->value();
    }

    /* check min and max length network parameters */
    if (((tmpIp.length() > 0) && (tmpMask.length() > 0) && (tmpGw.length() > 0) && (tmpDns.length() > 0)) && ((tmpIp.length() <= IPV4_ADDR_MAX_LENGTH) && (tmpMask.length() <= IPV4_ADDR_MAX_LENGTH) && (tmpGw.length() <= IPV4_ADDR_MAX_LENGTH) && (tmpDns.length() <= IPV4_ADDR_MAX_LENGTH))) {

      /* save ssid and password */
      SystemWifiMngt.SetNetworkConfig(tmpIp, tmpMask, tmpGw, tmpDns);

      /* send OK response */
      request->send(200, F("text/html"), MSG_SAVE_OK_REBOOT);

    } else {
      request->send(200, F("text/html"), MSG_SAVE_NOTOK);
    }
  });


  /* route for set basic auth
     /basicauth_cfg?auth_enable=true|false&auth_username=<u>[&auth_password=<p>]

     Everything is validated before anything is applied, so a rejected request
     changes nothing. The old handler copied each value into WebBasicAuth before
     checking it, so an over-long password failed to save but was still live in
     RAM until the next reboot, and the user was locked out by a password that
     was never stored.

     An empty or absent password keeps the stored one, so the SPA never has to
     send (or know) the current password just to change the username or toggle
     auth. Enabling auth with no password stored is refused: it would lock the
     page behind credentials nobody chose. */
  server.on("/basicauth_cfg", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /basicauth_cfg"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    bool enable = WebBasicAuth.EnableAuth;
    String user = WebBasicAuth.UserName;
    String pass = WebBasicAuth.Password;
    String err = "";

    if (request->hasParam("auth_username")) {
      user = request->getParam("auth_username")->value();
      if (!Server_IsValidAuthString(user, EEPROM_ADDR_BASIC_AUTH_USERNAME_LENGTH - 1, true)) {
        err = "Username: 1-" + String(EEPROM_ADDR_BASIC_AUTH_USERNAME_LENGTH - 1) + " printable characters, no ':'";
      }
    }

    if (err.isEmpty() && request->hasParam("auth_password")) {
      String tmp = request->getParam("auth_password")->value();
      if (tmp.length() > 0) {
        if (Server_IsValidAuthString(tmp, EEPROM_ADDR_BASIC_AUTH_PASSWORD_LENGTH - 1, false)) {
          pass = tmp;
        } else {
          err = "Password: 1-" + String(EEPROM_ADDR_BASIC_AUTH_PASSWORD_LENGTH - 1) + " printable characters";
        }
      }
    }

    /* accept the legacy parameter name as well */
    const char* enable_param = request->hasParam("auth_enable") ? "auth_enable" : "basicauth_enable";
    if (err.isEmpty() && request->hasParam(enable_param)) {
      enable = Server_TransfeStringToBool(request->getParam(enable_param)->value());
    }

    if (err.isEmpty() && enable && (user.isEmpty() || pass.isEmpty())) {
      err = "Set a username and password before enabling login";
    }

    if (!err.isEmpty()) {
      SystemLog.AddEvent(LogLevel_Warning, "Basic auth cfg rejected: " + err);
      request->send(400, F("text/plain"), String(MSG_SAVE_NOTOK) + " " + err);
      return;
    }

    if (user != WebBasicAuth.UserName) {
      WebBasicAuth.UserName = user;
      SystemConfig.SaveBasicAuthUsername(user);
    }
    if (pass != WebBasicAuth.Password) {
      WebBasicAuth.Password = pass;
      SystemConfig.SaveBasicAuthPassword(pass);
    }
    if (enable != WebBasicAuth.EnableAuth) {
      WebBasicAuth.EnableAuth = enable;
      SystemConfig.SaveBasicAuthFlag(enable);
    }
    SystemLog.AddEvent(LogLevel_Info, "Web login " + String(enable ? "enabled" : "disabled") + ", user: " + user);
    request->send(200, F("text/plain"), MSG_SAVE_OK);
  });


  /* route for set local hostname (mDNS name + DHCP hostname) /set_mdns?mdns=<name>
     The same name is used for both, so it has to be a valid RFC 1123 label —
     a router will not register "my cam" or "cam.local" as a DHCP hostname. */
  server.on("/set_mdns", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: /set_mdns"));
    if (Server_CheckBasicAuth(request) == false)
      return;

    if (!request->hasParam("mdns")) {
      request->send(400, F("text/plain"), "Missing mdns parameter");
      return;
    }
    String tmp = request->getParam("mdns")->value();
    if (WiFiMngt::IsValidHostname(tmp, EEPROM_ADDR_MDNS_RECORD_LENGTH - 1)) {
      SystemWifiMngt.SetMdns(tmp);
      request->send(200, F("text/plain"), MSG_SAVE_OK_REBOOT);
    } else {
      String msg = "Error save hostname. Use 1-" + String(EEPROM_ADDR_MDNS_RECORD_LENGTH - 1) +
                   " letters, digits or hyphens, not starting or ending with a hyphen";
      request->send(400, F("text/plain"), msg);
    }
  });
}

/**
   @brief Init WEB server stream
   @param none
   @return none
*/
void Server_InitWebServer_Stream() {
  server.on("/stream.mjpg", HTTP_GET, Server_streamJpg);
}

/**
   @brief Pause WEB server
   @param none
   @return none
*/
void Server_pause() {
  server.end();
  SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: pause"));
}

/**
   @brief Resume WEB server
   @param none
   @return none
*/
void Server_resume() {
  server.begin();
  SystemLog.AddEvent(LogLevel_Verbose, F("WEB server: resume"));
}

/**
 * @brief Handle cache request
 * 
 * @param AsyncWebServerRequest* - http request
 * @param const char* - content type
 * @param const char* - data
 */
/**
 * @brief Serve a pre-gzipped asset.
 *
 * The assets are stored compressed (see webpage/webpage_gz_generator.py) and handed
 * to the client as-is with Content-Encoding: gzip — no CPU or RAM cost at runtime,
 * the ESP32 just sends bytes it already holds. Every browser sends
 * Accept-Encoding: gzip, so there is no uncompressed fallback to maintain.
 *
 * This recovered ~82 KB of flash: the plain assets were ~105 KB of the image,
 * index.html alone being 58 KB.
 */
void Server_handleGzipRequest(AsyncWebServerRequest* request, const char* contentType,
                              const uint8_t* data, size_t len) {
  AsyncWebServerResponse* response = request->beginResponse(200, contentType, data, len);
  response->addHeader("Content-Encoding", "gzip");
  response->addHeader("Cache-Control", "public, max-age=" + String(WEB_CACHE_INTERVAL));
  request->send(response);
}

void Server_handleCacheRequest(AsyncWebServerRequest* request, const char* contentType, const char* data) {
  /*
  AsyncWebServerResponse* response = request->beginResponse(200, contentType, data);
  response->addHeader("Cache-Control", "public, max-age=" + String(WEB_CACHE_INTERVAL));
  request->send(response); 
  */

  size_t dataLen = strlen(data);
  AsyncWebServerResponse* response = request->beginChunkedResponse(contentType, [data, dataLen](uint8_t *buffer, size_t maxLen, size_t index) -> size_t {
    if (index >= dataLen) return 0;
    size_t dataLeft = dataLen - index;
    size_t chunkSize = dataLeft < maxLen ? dataLeft : maxLen;
    memcpy(buffer, data + index, chunkSize);
    return chunkSize;
  });
  response->addHeader("Cache-Control", "public, max-age=" + String(WEB_CACHE_INTERVAL));
  request->send(response);
}

/**
   @brief if the page was not found on ESP, then print which page is not there
   @param AsyncWebServerRequest* - request
   @return none
*/
void Server_handleNotFound(AsyncWebServerRequest* request) {
  String message = F("URL not Found\n\n");

  message += "URI: " + request->url() + "\nMethod: ";
  message += (request->method() == HTTP_GET) ? F("GET") : F("POST");
  message += "\nArguments: " + String(request->args()) + "\n";

  for (uint8_t i = 0; i < request->args(); i++) {
    message += " " + request->argName(i) + ": " + request->arg(i) + "\n";
  }

  request->send(404, F("text/plain"), message);
}

/**
   @brief make json data for WEB page on the ESP32
   @param none
   @return String - json data
*/
String Server_GetJsonData() {
  String uptime = "";
  String string_json = "";
  JsonDocument doc_json;
  Server_GetModuleUptime(uptime);

  /* Only expose credentials when the request was authenticated; without auth any
     LAN host polling json_input every 5 s would receive the Prusa Connect token. */
  if (WebBasicAuth.EnableAuth) {
    doc_json["token"] = Connect.GetToken();
    doc_json["fingerprint"] = Connect.GetFingerprint();
  } else {
    doc_json["token"] = "";
    doc_json["fingerprint"] = "";
  }
  doc_json["refreshInterval"] = String(Connect.GetRefreshInterval());
  doc_json["photoquality"] = String(73 - SystemCamera.GetPhotoQuality());
  doc_json["framesize"] = String(SystemCamera.GetFrameSize());
  doc_json["brightness"] = String(SystemCamera.GetBrightness());
  doc_json["contrast"] = String(SystemCamera.GetContrast());
  doc_json["saturation"] = String(SystemCamera.GetSaturation());
  doc_json["hmirror"] = Server_TranslateBoolToString(SystemCamera.GetHMirror());
  doc_json["vflip"] = Server_TranslateBoolToString(SystemCamera.GetVFlip());
  doc_json["lensc"] = Server_TranslateBoolToString(SystemCamera.GetLensC());
  doc_json["exposure_ctrl"] = Server_TranslateBoolToString(SystemCamera.GetExposureCtrl());
  doc_json["awb"] = Server_TranslateBoolToString(SystemCamera.GetAwb());
  doc_json["awb_gain"] = Server_TranslateBoolToString(SystemCamera.GetAwbGain());
  doc_json["wb_mode"] = String(SystemCamera.GetAwbMode());
  doc_json["bpc"] = Server_TranslateBoolToString(SystemCamera.GetBpc());
  doc_json["wpc"] = Server_TranslateBoolToString(SystemCamera.GetWpc());
  doc_json["raw_gama"] = Server_TranslateBoolToString(SystemCamera.GetRawGama());
  doc_json["aec2"] = Server_TranslateBoolToString(SystemCamera.GetAec2());
  doc_json["ae_level"] = SystemCamera.GetAeLevel();
  doc_json["aec_value"] = SystemCamera.GetAecValue();
  doc_json["gain_ctrl"] = Server_TranslateBoolToString(SystemCamera.GetGainCtrl());
  doc_json["agc_gain"] = SystemCamera.GetAgcGaint();
  doc_json["led"] = Server_TranslateBoolToString(SystemCamera.GetFlashStatus());
  doc_json["flash"] = Server_TranslateBoolToString(SystemCamera.GetCameraFlashEnable());
  doc_json["flash_time"] = SystemCamera.GetCameraFlashTime();
  doc_json["ssid"] = SystemWifiMngt.GetStaSsid();
  doc_json["bssid"] = SystemWifiMngt.GetStaBssid();
  doc_json["rssi"] = String(WiFi.RSSI());
  doc_json["rssi_percentage"] = String(SystemWifiMngt.Rssi2Percent(WiFi.RSSI()));
  doc_json["tx_power"] = SystemWifiMngt.TranslateTxPower(WiFi.getTxPower());
  doc_json["ip"] = WiFi.localIP().toString();
  doc_json["wifi_mode"] = SystemWifiMngt.GetWiFiMode();
  doc_json["mdns"] = SystemWifiMngt.GetMdns();
  doc_json["service_ap_ssid"] = SystemWifiMngt.GetServiceApSsid();
  doc_json["serviceap"] = Server_TranslateBoolToString(SystemWifiMngt.GetEnableServiceAp());
  doc_json["auth"] = Server_TranslateBoolToString(WebBasicAuth.EnableAuth);
  doc_json["auth_username"] = WebBasicAuth.UserName;
  doc_json["last_upload_status"] = Connect.GetBackendReceivedStatus();
  doc_json["uploading"] = Server_TranslateBoolToString(Connect.IsUploading());
  doc_json["wifi_network_status"] = SystemWifiMngt.GetStaStatus();
  doc_json["log_level"] = String(SystemLog.GetLogLevel());
  doc_json["uptime"] = uptime;
  doc_json["user_name"] = WebBasicAuth.UserName;
  doc_json["hostname"] = Connect.GetPrusaConnectHostname();
  doc_json["ip_cfg"] = SystemWifiMngt.GetNetIpMethod();
  doc_json["net_ip"] = SystemWifiMngt.GetNetStaticIp();
  doc_json["net_mask"] = SystemWifiMngt.GetNetStaticMask();
  doc_json["net_gw"] = SystemWifiMngt.GetNetStaticGateway();
  doc_json["net_dns"] = SystemWifiMngt.GetNetStaticDns();
  doc_json["image_rotation"] = SystemCamera.GetCameraImageRotation();
  doc_json["timelaps"] = Server_TranslateBoolToString(Connect.GetTimeLapsPhotoSaveStatus());
  doc_json["upload_enable"] = Server_TranslateBoolToString(Connect.GetUploadEnabled());
  /* Whether this board has an SD card at all, as opposed to having one that is
     currently empty. Without it the SPA cannot tell the two apart, and a board
     built with ENABLE_SD_CARD false (the WROVER) would show a timelapse UI whose
     every button returns 503. Fixed at build time, so the SPA reads it once. */
  doc_json["sd_hw"] = (bool)ENABLE_SD_CARD;
  doc_json["sd_status"] = (SystemLog.GetCardDetectedStatus() == true) ? F("Card detected") : F("No card detected");
  doc_json["sd_total"] = SystemLog.GetCardSizeMB();
  doc_json["sd_free_p"] = SystemLog.GetFreeSpacePercent();
  doc_json["sd_used_p"] = SystemLog.GetUsedSpacePercent();
  /* Bare number — the SPA appends the unit. Upstream's " *C" suffix stayed behind
     when the UI started adding " °C" of its own, and the card read "105.00 *C °C". */
  doc_json["mcu_temp"] = String(McuTemperature.TemperatureCelsius);
  doc_json["sw_build"] = SW_BUILD;
  doc_json["sw_ver"] = SW_VERSION;
  doc_json["sw_new_ver"] = "";
  doc_json["pl_enable"]              = Server_TranslateBoolToString(SystemPrusaLink.GetEnable());
  doc_json["pl_stop_upload_on_done"] = Server_TranslateBoolToString(SystemPrusaLink.GetStopUploadOnDone());
  doc_json["pl_ip"]          = SystemPrusaLink.GetIp();
  doc_json["pl_last_state"]  = SystemPrusaLink.GetLastState();
  doc_json["tl_trigger"]     = SystemPrusaLink.GetTriggerMode();
  doc_json["pl_reachable"]   = Server_TranslateBoolToString(SystemPrusaLink.GetReachable());
  doc_json["pl_progress"]    = SystemPrusaLink.GetProgress();
  doc_json["pl_nozzle"]      = SystemPrusaLink.GetNozzleTemp();
  doc_json["pl_bed"]         = SystemPrusaLink.GetBedTemp();
  doc_json["tl_recording"]   = Server_TranslateBoolToString(SystemTimelapse.isRecording());
  doc_json["tl_frame_count"] = SystemTimelapse.frameCount();
  doc_json["tl_file_size_kb"] = (uint32_t)(SystemTimelapse.fileSizeBytes() / 1024);
  doc_json["tl_file_name"]   = SystemTimelapse.currentFileName();
  doc_json["tl_max_frames"]  = SystemTimelapse.maxFrames();

  serializeJson(doc_json, string_json);
  SystemLog.AddEvent(LogLevel_Verbose, string_json);
  return string_json;
}

/**
   @brief If basic auth is enabled, then check if the user is logged in
   @param AsyncWebServerRequest - request
   @return bool - status
*/
bool Server_CheckBasicAuth(AsyncWebServerRequest* request) {
  if ((!request->authenticate(WebBasicAuth.UserName.c_str(), WebBasicAuth.Password.c_str())) && (true == WebBasicAuth.EnableAuth)) {
    SystemLog.AddEvent(LogLevel_Verbose, F("Unauthorized! Sending longin request"));
    request->requestAuthentication();
    return false;
  }

  return true;
}

/**
   @brief Validate a basic-auth username or password
   @param String - value
   @param size_t - maximum length in bytes
   @param bool - true for a username, which may not contain ':' (RFC 7617 splits
                 "user:pass" on the first colon, so such a user could never log in)
   @return bool - true when the value is 1..max printable ASCII characters
*/
bool Server_IsValidAuthString(const String& i_data, size_t i_max, bool i_username) {
  if ((i_data.length() == 0) || (i_data.length() > i_max))
    return false;
  for (size_t i = 0; i < i_data.length(); i++) {
    char c = i_data.charAt(i);
    if ((c < 0x20) || (c > 0x7E))
      return false;
    if (i_username && (c == ':'))
      return false;
  }
  return true;
}

/**
   @brief Stream JPG image from camera
   @param AsyncWebServerRequest - request
   @return void
*/
void Server_streamJpg(AsyncWebServerRequest* request) {
  /* The stream was the one route that skipped the login check, so enabling auth
     still left the live camera view open to the whole LAN. */
  if (Server_CheckBasicAuth(request) == false)
    return;
  AsyncJpegStreamResponse* response = new AsyncJpegStreamResponse(&SystemCamera, &SystemLog);
  if (!response) {
    request->send(501);
    return;
  }
  response->addHeader("Access-Control-Allow-Origin", "*");
  request->send(response);
}

/**
   @brief Get module uptime
   @param String - uptime value
   @return void
*/
void Server_GetModuleUptime(String& readableTime) {
  unsigned long currentMillis;
  unsigned long seconds;
  unsigned long minutes;
  unsigned long hours;
  unsigned long days;

  currentMillis = millis();
  seconds = currentMillis / 1000;
  minutes = seconds / 60;
  hours = minutes / 60;
  days = hours / 24;
  currentMillis %= 1000;
  seconds %= 60;
  minutes %= 60;
  hours %= 24;

  readableTime = String(days) + " days, ";

  if (hours < 10) {
    readableTime += "0";
  }
  readableTime += String(hours) + ":";

  if (minutes < 10) {
    readableTime += "0";
  }
  readableTime += String(minutes) + ":";

  if (seconds < 10) {
    readableTime += "0";
  }
  readableTime += String(seconds);
}

/**
   @brief Convert string bool variable to bool
   @param String - data
   @return bool - status
*/
bool Server_TransfeStringToBool(String data) {
  return data == "true" || data == "1";
}

String Server_TranslateBoolToString(bool i_data) {
  return i_data ? "true" : "false";
}

/**
   @brief Timelapse REST API endpoints
   Routes: /timelapse/start  /timelapse/stop  /timelapse/status
           /timelapse/list   /timelapse/download  /timelapse/delete
*/
void Server_InitWebServer_Timelapse() {

  /* GET /timelapse/start — begin a new AVI recording */
  server.on("/timelapse/start", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /timelapse/start"));
    if (Server_CheckBasicAuth(request) == false) return;

#if (ENABLE_SD_CARD == true)
    if (!SystemLog.GetCardDetectedStatus()) {
      request->send(503, "text/plain", "SD card not available");
      return;
    }
    if (SystemTimelapse.isRecording()) {
      request->send(400, "text/plain", "Already recording");
      return;
    }
    CaptureCommand_t tlStartCmd = CMD_TL_START;
    xQueueSend(g_captureQueue, &tlStartCmd, 0);
    request->send(200, "text/plain", "Timelapse start queued");
#else
    request->send(501, "text/plain", "SD card not enabled");
#endif
  });

  /* GET /timelapse/stop — finalise current AVI */
  server.on("/timelapse/stop", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /timelapse/stop"));
    if (Server_CheckBasicAuth(request) == false) return;

#if (ENABLE_SD_CARD == true)
    if (!SystemTimelapse.isRecording()) {
      request->send(400, "text/plain", "Not recording");
      return;
    }
    String currentFile = SystemTimelapse.currentFileName();
    CaptureCommand_t tlStopCmd = CMD_TL_STOP;
    xQueueSendToFront(g_captureQueue, &tlStopCmd, pdMS_TO_TICKS(200));
    request->send(200, "text/plain", "Timelapse stop queued: " + currentFile);
#else
    request->send(501, "text/plain", "SD card not enabled");
#endif
  });

  /* GET /timelapse/status — JSON recording status */
  server.on("/timelapse/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /timelapse/status"));
    if (Server_CheckBasicAuth(request) == false) return;

    JsonDocument doc;
    doc["recording"]   = SystemTimelapse.isRecording();
    doc["frameCount"]  = SystemTimelapse.frameCount();
    doc["fileSizeMB"]  = (float)SystemTimelapse.fileSizeBytes() / (1024.0f * 1024.0f);
    doc["fileName"]    = SystemTimelapse.currentFileName();
#if (ENABLE_SD_CARD == true)
    doc["sdFreeMB"]    = SystemLog.GetFreeSpaceMB();
#endif
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  /* GET /timelapse/list — JSON array of AVI files on SD card */
  server.on("/timelapse/list", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /timelapse/list"));
    if (Server_CheckBasicAuth(request) == false) return;

#if (ENABLE_SD_CARD == true)
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();

    /* Collect first, then sort newest-first. FAT returns directory entries in
       creation order on a fresh card but in free-slot order once files have been
       deleted, so raw enumeration order is not chronological and cannot be relied on.
       Sorting on the modification time from the filesystem is authoritative; the
       filename timestamp is only a display convenience. */
    struct TlEntry { String name; uint32_t size; time_t mtime; };
    std::vector<TlEntry> files;
    files.reserve(16);

    File dir = SD_MMC.open(TIMELAPS_AVI_FOLDER);
    if (dir && dir.isDirectory()) {
      File entry = dir.openNextFile();
      while (entry && files.size() < TL_LIST_MAX_FILES) {
        String name = String(entry.name());
        if (!entry.isDirectory() && name.endsWith(TIMELAPS_AVI_SUFFIX)) {
          files.push_back({ name, (uint32_t)entry.size(), entry.getLastWrite() });
        }
        entry.close();
        entry = dir.openNextFile();
      }
      dir.close();
    }

    std::sort(files.begin(), files.end(), [](const TlEntry &a, const TlEntry &b) {
      if (a.mtime != b.mtime) return a.mtime > b.mtime;   /* newest first */
      return a.name > b.name;                             /* stable tie-break */
    });

    for (const TlEntry &f : files) {
      JsonObject obj = arr.add<JsonObject>();
      obj["name"]   = f.name;
      obj["sizeMB"] = (float)f.size / (1024.0f * 1024.0f);
      obj["mtime"]  = (uint32_t)f.mtime;
      /* Pre-formatted so the browser does not have to know the epoch convention.
         FAT stamps are only meaningful if NTP had synced when the file was written;
         anything before 2000 means it had not, so report it as unknown rather than
         showing a bogus 1980 date. */
      char ds[24] = { 0 };
      if (f.mtime > 946684800) {   /* 2000-01-01 */
        struct tm tmv;
        localtime_r(&f.mtime, &tmv);
        strftime(ds, sizeof(ds), "%Y-%m-%d %H:%M", &tmv);
      }
      obj["date"] = ds;
    }

    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
#else
    request->send(501, "text/plain", "SD card not enabled");
#endif
  });

  /* GET /timelapse/download?file=tl_20250516_143022.avi — stream AVI file.
     Sets the download-active flag for the duration so snapshot uploads pause. */
  server.on("/timelapse/download", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /timelapse/download"));
    if (Server_CheckBasicAuth(request) == false) return;

#if (ENABLE_SD_CARD == true)
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing ?file= parameter");
      return;
    }
    String fname = request->getParam("file")->value();
    /* basic path traversal guard */
    if (fname.indexOf("..") >= 0 || fname.indexOf("/") >= 0) {
      request->send(400, "text/plain", "Invalid filename");
      return;
    }
    String path = String(TIMELAPS_AVI_FOLDER) + "/" + fname;
    File& dlFile = SystemTimelapse.getDownloadFile();
    dlFile = SD_MMC.open(path.c_str(), FILE_READ);
    if (!dlFile) {
      request->send(404, "text/plain", "File not found");
      return;
    }
    size_t fsize = dlFile.size();

    /* ---- HTTP Range ----------------------------------------------------------
       ESPAsyncWebServer does not implement ranges; it hardcodes "Accept-Ranges:
       none" in _assembleHead(). So a dropped transfer of a 40 MB file previously
       had to restart from zero. Implemented here by hand: the library adds its
       "none" with replaceExisting = false, so advertising "bytes" first wins.
       Forms handled: "bytes=S-E", "bytes=S-" and the suffix form "bytes=-N". */
    size_t rangeStart = 0;
    size_t rangeEnd   = (fsize > 0) ? (fsize - 1) : 0;
    bool   isRange    = false;

    if (request->hasHeader("Range") && fsize > 0) {
      String r = request->getHeader("Range")->value();
      r.trim();
      if (r.startsWith("bytes=")) {
        String spec = r.substring(6);
        int dash = spec.indexOf('-');
        if (dash >= 0) {
          String sS = spec.substring(0, dash);
          String sE = spec.substring(dash + 1);
          sS.trim(); sE.trim();
          bool ok = true;
          if (sS.length() == 0) {
            /* suffix form: last N bytes */
            size_t n = (size_t)sE.toInt();
            if (n == 0 || sE.length() == 0) ok = false;
            else { rangeStart = (n >= fsize) ? 0 : (fsize - n); rangeEnd = fsize - 1; }
          } else {
            rangeStart = (size_t)sS.toInt();
            rangeEnd   = (sE.length() > 0) ? (size_t)sE.toInt() : (fsize - 1);
            if (rangeEnd >= fsize) rangeEnd = fsize - 1;
            if (rangeStart > rangeEnd) ok = false;
          }
          isRange = ok;
        }
      }
      if (!isRange || rangeStart >= fsize) {
        /* Unsatisfiable — must answer 416 with the real length so the client can
           recover, not 200 with the whole file. */
        dlFile.close();
        AsyncWebServerResponse* r416 = request->beginResponse(416, "text/plain", "Range Not Satisfiable");
        r416->addHeader("Content-Range", "bytes */" + String(fsize));
        r416->addHeader("Accept-Ranges", "bytes");
        request->send(r416);
        return;
      }
    }

    const size_t sendLen = rangeEnd - rangeStart + 1;

    /* One seek, here — not per chunk. Per-chunk position()/seek() was measured at
       35 KB/s versus 218 KB/s; reads after this are sequential. */
    if (rangeStart > 0 && !dlFile.seek(rangeStart)) {
      dlFile.close();
      request->send(500, "text/plain", "Seek failed");
      return;
    }

    SystemTimelapse.setDownloadActive(true, sendLen);
    SystemLog.AddEvent(LogLevel_Info, "Timelapse download: " + fname + " "
                                      + (isRange ? ("range " + String(rangeStart) + "-" + String(rangeEnd)) : String("full"))
                                      + " of " + String(fsize) + " bytes");

    /* Sized (not chunked) response: a real Content-Length is what lets a client
       detect truncation and resume from the right offset. */
    auto response = request->beginResponse("video/avi", sendLen,
      [sendLen](uint8_t* buf, size_t maxLen, size_t index) -> size_t {
        if (index >= sendLen) {
          SystemTimelapse.setDownloadActive(false);
          SystemTimelapse.getDownloadFile().close();
          return 0;
        }

        size_t toRead = min(maxLen, sendLen - index);
        File& f = SystemTimelapse.getDownloadFile();
        if (!f) return 0;

        /* Serialise against the recorder ONLY while it is actually recording — with
           no print running there is nothing to contend with, and taking the mutex on
           every chunk is pure overhead on the hot path.
           No seek() here on purpose: reads are sequential, and calling position()/
           seek() per chunk costs a VFS round-trip each. Measured at 35 KB/s with
           them versus 218 KB/s without, slow enough that the server's ack timeout
           closed the connection and truncated the file. */
        size_t got = 0;
        if (SystemTimelapse.isRecording()) {
          /* A live print outranks a transfer of an old file: if the recorder holds
             the mutex, defer this chunk rather than block the AsyncTCP task.
             RESPONSE_TRY_AGAIN is the only correct way to say "not now" — returning 0
             would end the response and silently truncate the download. */
          if (!SystemTimelapse.lockForBulkOp(pdMS_TO_TICKS(TL_DOWNLOAD_LOCK_WAIT_MS))) {
            SystemTimelapse.noteDownloadProgress();  /* deferring is not stalling */
            return RESPONSE_TRY_AGAIN;
          }
          got = f.read(buf, toRead);
          SystemTimelapse.unlockForBulkOp();
        } else {
          got = f.read(buf, toRead);
        }

        SystemTimelapse.noteDownloadProgress();
        return got;
      });
    /* Added before the library assembles the head, so its "Accept-Ranges: none"
       (emitted with replaceExisting = false) does not override this. */
    response->addHeader("Accept-Ranges", "bytes");
    response->addHeader("Content-Disposition", "attachment; filename=\"" + fname + "\"");
    if (isRange) {
      response->setCode(206);
      response->addHeader("Content-Range", "bytes " + String(rangeStart) + "-"
                                           + String(rangeEnd) + "/" + String(fsize));
    }

    /* Without this, an aborted download left the flag set until its estimated
       deadline expired — up to ~17 minutes for a 43 MB file, during which capture
       and uploads were suppressed. */
    request->onDisconnect([]() {
      if (SystemTimelapse.isDownloadActive()) {
        SystemLog.AddEvent(LogLevel_Info, F("Timelapse download: client disconnected, releasing"));
      }
      SystemTimelapse.setDownloadActive(false);
      SystemTimelapse.getDownloadFile().close();
    });

    request->send(response);
#else
    request->send(501, "text/plain", "SD card not enabled");
#endif
  });

  /* GET /timelapse/delete?file=tl_001.avi — delete AVI file */
  server.on("/timelapse/delete", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /timelapse/delete"));
    if (Server_CheckBasicAuth(request) == false) return;

#if (ENABLE_SD_CARD == true)
    if (!request->hasParam("file")) {
      request->send(400, "text/plain", "Missing ?file= parameter");
      return;
    }
    String fname = request->getParam("file")->value();
    if (fname.indexOf("..") >= 0 || fname.indexOf("/") >= 0) {
      request->send(400, "text/plain", "Invalid filename");
      return;
    }
    if (SystemTimelapse.isDownloadActive()) {
      request->send(409, "text/plain", "Download in progress");
      return;
    }
    if (!SystemLog.TakeSdMutex(pdMS_TO_TICKS(200))) {
      request->send(503, "text/plain", "SD busy");
      return;
    }
    String path = String(TIMELAPS_AVI_FOLDER) + "/" + fname;
    if (!SD_MMC.exists(path.c_str())) {
      SystemLog.GiveSdMutex();
      request->send(404, "text/plain", "File not found");
      return;
    }
    SystemLog.DeleteFile(SD_MMC, path);
    SystemLog.GiveSdMutex();
    request->send(200, "text/plain", "Deleted: " + fname);
#else
    request->send(501, "text/plain", "SD card not enabled");
#endif
  });

  /* GET /timelapse/delete_all — delete every AVI in the timelapse folder */
  server.on("/timelapse/delete_all", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /timelapse/delete_all"));
    if (Server_CheckBasicAuth(request) == false) return;

#if (ENABLE_SD_CARD == true)
    if (SystemTimelapse.isDownloadActive()) {
      request->send(409, "text/plain", "Download in progress");
      return;
    }
    /* C3: hold _mutex for the full operation so begin() cannot open a new file
       between the isRecording() check and the first delete. */
    if (!SystemTimelapse.lockForBulkOp(pdMS_TO_TICKS(500))) {
      request->send(503, "text/plain", "Timelapse busy");
      return;
    }
    if (SystemTimelapse.isRecording()) {
      SystemTimelapse.unlockForBulkOp();
      request->send(409, "text/plain", "Cannot delete while recording");
      return;
    }
    /* C1: hold SD mutex for the entire directory walk + delete sequence */
    if (!SystemLog.TakeSdMutex(pdMS_TO_TICKS(2000))) {
      SystemTimelapse.unlockForBulkOp();
      request->send(503, "text/plain", "SD busy");
      return;
    }
    int deleted = 0;
    File dir = SD_MMC.open(TIMELAPS_AVI_FOLDER);
    if (dir && dir.isDirectory()) {
      File entry = dir.openNextFile();
      while (entry) {
        String name  = String(entry.name());
        bool   isDir = entry.isDirectory();
        entry.close();
        if (!isDir && name.endsWith(TIMELAPS_AVI_SUFFIX)) {
          String path = String(TIMELAPS_AVI_FOLDER) + "/" + name;
          SystemLog.DeleteFile(SD_MMC, path);
          deleted++;
        }
        entry = dir.openNextFile();
      }
      dir.close();
    }
    SystemLog.GiveSdMutex();
    SystemTimelapse.unlockForBulkOp();
    SystemLog.AddEvent(LogLevel_Info, F("Timelapse: delete_all, count="), String(deleted));
    request->send(200, "text/plain", "Deleted " + String(deleted) + " file(s)");
#else
    request->send(501, "text/plain", "SD card not enabled");
#endif
  });
}

/**
   @brief PrusaLink configuration and status endpoints
   Routes: /set_prusa_link_enable  /set_prusa_link_ip
           /set_prusa_link_key     /prusa_link_status
*/
void Server_InitWebServer_PrusaLink() {

  /* GET /set_prusa_link_enable?val=true|false */
  server.on("/set_prusa_link_enable", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /set_prusa_link_enable"));
    if (Server_CheckBasicAuth(request) == false) return;
    if (!request->hasParam("val")) { request->send(400, "text/plain", "Missing ?val="); return; }
    bool v = Server_TransfeStringToBool(request->getParam("val")->value());
    SystemPrusaLink.SetEnable(v);
    request->send(200, "text/plain", "PrusaLink enable: " + String(v));
  });

  /* GET /set_prusa_link_ip?val=192.168.1.50 */
  server.on("/set_prusa_link_ip", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /set_prusa_link_ip"));
    if (Server_CheckBasicAuth(request) == false) return;
    if (!request->hasParam("val")) { request->send(400, "text/plain", "Missing ?val="); return; }
    SystemPrusaLink.SetIp(request->getParam("val")->value());
    request->send(200, "text/plain", "PrusaLink IP saved");
  });

  /* GET /set_prusa_link_key?val=<api-key> */
  server.on("/set_prusa_link_key", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /set_prusa_link_key"));
    if (Server_CheckBasicAuth(request) == false) return;
    if (!request->hasParam("val")) { request->send(400, "text/plain", "Missing ?val="); return; }
    SystemPrusaLink.SetApiKey(request->getParam("val")->value());
    request->send(200, "text/plain", "PrusaLink API key saved");
  });

  /* GET /set_tl_max_frames?val=N — 0 = unlimited */
  server.on("/set_tl_max_frames", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /set_tl_max_frames"));
    if (Server_CheckBasicAuth(request) == false) return;
    if (!request->hasParam("val")) { request->send(400, "text/plain", "Missing ?val="); return; }
    uint16_t val = (uint16_t)request->getParam("val")->value().toInt();
    SystemConfig.SaveTlMaxFrames(val);
    SystemTimelapse.setMaxFrames(val);
    request->send(200, "text/plain", "TL max frames: " + String(val));
  });

  /* GET /set_tl_trigger?val=manual|printer */
  server.on("/set_tl_trigger", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /set_tl_trigger"));
    if (Server_CheckBasicAuth(request) == false) return;
    if (!request->hasParam("val")) { request->send(400, "text/plain", "Missing ?val="); return; }
    String val = request->getParam("val")->value();
    uint8_t mode = (val == "printer") ? TL_TRIGGER_PRUSA_LINK : TL_TRIGGER_MANUAL;
    SystemPrusaLink.SetTriggerMode(mode);
    request->send(200, "text/plain", "TL trigger: " + val);
  });

  /* GET /set_pl_stop_upload_on_done?val=true|false */
  server.on("/set_pl_stop_upload_on_done", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /set_pl_stop_upload_on_done"));
    if (Server_CheckBasicAuth(request) == false) return;
    if (!request->hasParam("val")) { request->send(400, "text/plain", "Missing ?val="); return; }
    bool v = Server_TransfeStringToBool(request->getParam("val")->value());
    SystemPrusaLink.SetStopUploadOnDone(v);
    request->send(200, "text/plain", "pl_stop_upload_on_done: " + String(v));
  });

  /* GET /prusa_link_status — JSON status */
  server.on("/prusa_link_status", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Verbose, F("WEB: /prusa_link_status"));
    if (Server_CheckBasicAuth(request) == false) return;
    JsonDocument doc;
    doc["enable"]      = SystemPrusaLink.GetEnable();
    doc["ip"]          = SystemPrusaLink.GetIp();
    doc["lastState"]   = SystemPrusaLink.GetLastState();
    doc["triggerMode"] = SystemPrusaLink.GetTriggerMode();
    doc["reachable"]   = SystemPrusaLink.GetReachable();
    doc["frameMode"]   = SystemPrusaLink.GetFrameMode();
    doc["axisZ"]       = SystemPrusaLink.GetAxisZ();
    doc["layerFrames"] = SystemPrusaLink.GetLayerFrames();
    doc["pollMs"]      = SystemPrusaLink.PollIntervalMs();
    doc["progress"]    = SystemPrusaLink.GetProgress();
    doc["layerHeight"] = SystemPrusaLink.GetLayerHeight();
    doc["jobName"]     = SystemPrusaLink.GetJobName();
    String out;
    serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  /* ---- OTA: on-demand only, never polled ----
     Handlers just raise a flag; the HTTPS work happens on the housekeeping task.
     Doing TLS here would stall every other request and overflow the AsyncTCP stack. */
  server.on("/ota/check", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (Server_CheckBasicAuth(request) == false) return;
    if (SystemOta.Busy()) { request->send(409, "text/plain", "busy"); return; }
    SystemLog.AddEvent(LogLevel_Info, F("WEB: /ota/check"));
    SystemOta.RequestCheck();
    request->send(200, "text/plain", "checking");
  });

  server.on("/ota/install", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (Server_CheckBasicAuth(request) == false) return;
    SystemLog.AddEvent(LogLevel_Warning, F("WEB: /ota/install"));
    if (!SystemOta.RequestInstall()) {
      request->send(409, "text/plain", SystemOta.GetMessage());
      return;
    }
    request->send(200, "text/plain", "installing");
  });

  server.on("/ota/status", HTTP_GET, [](AsyncWebServerRequest* request) {
    if (Server_CheckBasicAuth(request) == false) return;
    JsonDocument doc;
    doc["state"]    = (int)SystemOta.GetState();
    doc["message"]  = SystemOta.GetMessage();
    doc["latest"]   = SystemOta.GetLatest();
    doc["current"]  = SW_VERSION;
    doc["progress"] = SystemOta.GetProgress();
    doc["busy"]     = SystemOta.Busy();
    String out; serializeJson(doc, out);
    request->send(200, "application/json", out);
  });

  /* GET /set_tl_frame_mode?val=time|layer
     "layer" emits one AVI frame per detected layer change instead of one per photo
     cycle. Requires PrusaLink enabled — axis_z is the only source of layer info. */
  server.on("/set_tl_frame_mode", HTTP_GET, [](AsyncWebServerRequest* request) {
    SystemLog.AddEvent(LogLevel_Info, F("WEB: /set_tl_frame_mode"));
    if (Server_CheckBasicAuth(request) == false) return;
    if (!request->hasParam("val")) {
      request->send(400, "text/plain", "missing val (time|layer)");
      return;
    }
    String v = request->getParam("val")->value();
    if (v == "layer") {
      if (!SystemPrusaLink.GetEnable()) {
        request->send(409, "text/plain", "enable PrusaLink first - axis_z is the layer source");
        return;
      }
      SystemPrusaLink.SetFrameMode(TL_FRAME_LAYER);
    } else if (v == "time") {
      SystemPrusaLink.SetFrameMode(TL_FRAME_TIME);
    } else {
      request->send(400, "text/plain", "val must be time or layer");
      return;
    }
    request->send(200, "text/plain", "tl_frame_mode=" + v);
  });
}

/* EOF */
