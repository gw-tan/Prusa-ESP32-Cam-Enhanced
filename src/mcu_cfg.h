/**
   @file mcu_cfg.h

   @brief Library configuration MCU

   @author Miroslav Pivovarsky
   Contact: miroslav.pivovarsky@gmail.com

   @bug: no know bug
*/

#ifndef _MCU_CFG_H_
#define _MCU_CFG_H_

/* ----------------- CAMERA TYPE  ---------------*/
/* The board comes from the PlatformIO environment's build_flags (-DBOARD=true),
   and module_templates.h selects the header by VALUE, not by #ifdef. So these
   must be #ifndef-guarded defaults: defining them unconditionally, as this file
   used to, silently overwrote the -D from platformio.ini and every environment
   built AI Thinker firmware regardless of the one it named — including a
   "firmware-wrover.bin" that was AI Thinker inside.

   No default board. A build with no flag is a mistake and must fail, which the
   guard in module_templates.h does. Never reintroduce a fallback here. */
#ifndef AI_THINKER_ESP32_CAM
#define AI_THINKER_ESP32_CAM            false
#endif
#ifndef ESP32_WROVER_DEV
#define ESP32_WROVER_DEV                false
#endif
#ifndef CAMERA_MODEL_ESP32_S3_DEV_CAM
#define CAMERA_MODEL_ESP32_S3_DEV_CAM   false
#endif
#ifndef CAMERA_MODEL_ESP32_S3_EYE_2_2
#define CAMERA_MODEL_ESP32_S3_EYE_2_2   false
#endif
#ifndef CAMERA_MODEL_XIAO_ESP32_S3_CAM
#define CAMERA_MODEL_XIAO_ESP32_S3_CAM  false
#endif
#ifndef CAMERA_MODEL_ESP32_S3_CAM
#define CAMERA_MODEL_ESP32_S3_CAM       false
#endif
#ifndef ESP32_S3_WROOM_FREENOVE
#define ESP32_S3_WROOM_FREENOVE         false
#endif

/* ---------------- BASIC MCU CFG  --------------*/
/* Versioned independently of upstream from here on. This fork has diverged far
   enough — reliability fixes, per-layer timelapse, on-demand OTA, resumable
   downloads — that reusing prusa3d's numbering would imply a correspondence that no
   longer exists. Restarted at 1.0.0 as its own project.
   The release workflow fails the build if a git tag disagrees with this string. */
#define SW_VERSION                  "1.2.0"                 ///< SW version
#define SW_BUILD                    __DATE__ " " __TIME__   ///< build number
#define CONSOLE_VERBOSE_DEBUG       false                   ///< enable/disable verbose debug log level for console
#define DEVICE_HOSTNAME             "Prusa-ESP32cam"        ///< device hostname
#define CAMERA_MAX_FAIL_CAPTURE     10                      ///< maximum count for failed capture
#define CAMERA_INIT_MAX_ATTEMPTS    3                       ///< esp_camera_init() retries before giving up
#define CAMERA_INIT_RETRY_DELAY_MS  250                     ///< delay between camera init retries [ms]
#define CAMERA_SNAPSHOT_WAIT_MS     2000                    ///< max wait for the frame buffer when staging an upload snapshot [ms]

/* ------------ PRUSA BACKEND CFG  --------------*/
#define HOST_URL_CAM_PATH           "/c/snapshot"           ///< path for sending photo to prusa connect
#define HOST_URL_INFO_PATH          "/c/info"               ///< path for sending info to prusa connect
#define REFRESH_INTERVAL_MIN        10                      ///< minimum refresh interval for sending photo to prusa connect [s]
#define REFRESH_INTERVAL_MAX        240                     ///< maximum refresh interval for sending photo to prusa connect [s]

/* Upload timeouts. Every value below must keep (socket + handshake + response)
   comfortably under WDG_TIMEOUT, or a slow server reboots the MCU. */
#define CONNECT_SOCKET_TIMEOUT_MS       8000    ///< NetworkClient::setConnectionTimeout [ms]. SO_SNDTIMEO/SO_RCVTIMEO + send_ssl_data budget. Framework default 30000
#define CONNECT_HANDSHAKE_TIMEOUT_S     12      ///< NetworkClientSecure::setHandshakeTimeout [SECONDS]. Framework default 120 s > WDG_TIMEOUT
#define CONNECT_STREAM_PARSE_TIMEOUT_MS 5000    ///< Stream::setTimeout [ms]. Only affects readStringUntil() in the response loop
#define CONNECT_RESPONSE_TIMEOUT_MS     8000    ///< wall-clock cap on reading the HTTP response [ms]
#define UPLOAD_MIN_THROUGHPUT_BPS       24000   ///< conservative TLS throughput floor used to size the body-write budget [bytes/s]
#define UPLOAD_BODY_BUDGET_MIN_MS       15000   ///< body-write budget floor [ms]; must exceed CONNECT_SOCKET_TIMEOUT_MS
#define UPLOAD_BODY_BUDGET_MAX_MS       45000   ///< body-write budget ceiling [ms]
#define UPLOAD_MAX_ATTEMPTS             3       ///< maximum upload attempts per cycle
#define UPLOAD_CYCLE_GUARD_S            8       ///< reserve this much of the refresh interval so retries never overrun the next cycle [s]
#define MBEDTLS_PSRAM_THRESHOLD         512     ///< mbedTLS allocations >= this go to PSRAM first, keeping internal RAM free for FreeRTOS objects [bytes]

/* -------------- STATUS LED CFG ----------------*/
#define STATUS_LED_ON_DURATION      100                     ///< time for blink status LED when is module in the ON state [ms]
#define STATUS_LED_WIFI_AP          400                     ///< time for blink status LED when is module in the AP mode [ms]
#define STATUS_LED_STA_CONNECTING   800                     ///< time for blink status LED when is module connecting to the WiFi network [ms]
#define STATUS_LED_STA_CONNECTED    4000                    ///< time for blink status LED when is module connected to the WiFi network [ms]
#define STATUS_LED_ERROR            100                     ///< time for blink status LED when is module in the error state [ms]

/* ------------------- TASKS --------------------*/
#define TASK_SYSTEM                 100                     ///< system task interval [ms] — 100ms for responsive button polling
#define TASK_HOUSEKEEPING           100                     ///< housekeeping scheduler tick [ms]; all periods below are multiples of it
#define TASK_SDCARD                 25000                   ///< sd card task interval [ms]
#define NTP_RETRY_MS                5000                    ///< first-NTP-sync retry interval; independent of TASK_WIFI, which is for WiFi housekeeping
#define TASK_WIFI                   28000                   ///< wifi reconnect interval. Checking when is signal lost [ms]
#define TASK_SERIAL_CFG             1000                    ///< serial cfg task interval [ms]
#define TASK_SYSTEM_TELEMETRY       30000                   ///< stream telemetry task interval [ms]
#define TASK_WIFI_WATCHDOG          20000                   ///< wifi watchdog task interval [ms]
#define TASK_PHOTO_SEND             1000                    ///< photo send task interval [ms]
#define TASK_SDCARD_FILE_REMOVE     30000                   ///< sd card file remove task interval [ms]

/* --------------- WEB SERVER CFG  --------------*/
#define WEB_SERVER_PORT             80                      ///< WEB server port 
#define SERIAL_PORT_SPEED           115200                  ///< baud rate 
#define WDG_TIMEOUT                 60000                   ///< wdg timeout [ms]
#define PHOTO_FRAGMENT_SIZE         2048                    ///< photo fragmentation size [bytes]
#define LOOP_DELAY                  100                     ///< loop delay [ms]
#define WIFI_CLIENT_WAIT_CON        false                   ///< wait for connecting to WiFi network
#define WEB_CACHE_INTERVAL          86400                   ///< cache interval for browser [s] 86400s = 24h


/* ---------- RESET CFG CONFIGURATION  ----------*/
#define CFG_RESET_TIME_WAIT         10000                   ///< wait to 10 000 ms = 10s for reset cfg during grounded CFG_RESET_PIN 
#define CFG_RESET_LOOP_DELAY        100                     ///< delay in the loop for reset cfg

/* ---------------- MicroSD Logs ----------------*/
#define LOGS_FILE_NAME              "SysLog.log"            ///< syslog file name
#define LOGS_FILE_PATH              "/"                     ///< directory for log files
#define LOGS_FILE_MAX_SIZE          1024                    ///< maximum file size in the [kb]
#define FILE_REMOVE_MAX_COUNT       5                       ///< maximum count for remove files from sd card

/* ---------------- AP MODE CFG  ----------------*/
#define STA_AP_MODE_TIMEOUT         300000                  ///< how long is AP enable after start, when is module in the STA mode [ms]
#define SERVICE_WIFI_SSID_UID       true                    ///< enable/disable added UID to service SSID name
#define SERVICE_WIFI_SSID           F("ESP32_camera")       ///< service WI-FI SSID name. Maximum length SERVICE_WIFI_SSID + UID = 32
#define SERVICE_WIFI_PASS           F("12345678")           ///< service WI-FI password
#define SERVICE_WIFI_CHANNEL        10                      ///< service WI-FI channel
#define SERVICE_LOCAL_IP            F("192.168.0.1")        ///< service WI-FI module IP address
#define SERVICE_LOCAL_GATEWAY       F("192.168.0.1")        ///< service WI-FI module gateway
#define SERVICE_LOCAL_MASK          F("255.255.255.0")      ///< service WI-FI module mask
#define SERVICE_LOCAL_DNS           F("192.168.0.1")        ///< service WI-FI module DNS

/* ----------------- IPv4 CFG -------------------*/
#define IPV4_ADDR_MAX_LENGTH        15                      ///< maximum length for IPv4 address

/* ----------------- WiFi CFG -------------------*/
#define WIFI_STA_WDG_TIMEOUT        60000                   ///< STA watchdog timeout [ms]
/* STA loss recovery ladder — cheap recoveries first, reboot only as last resort */
/* wifi_power_t values from WiFiGeneric.h: 78=19.5dBm(max) 68=17 60=15 52=13 44=11.
   0 = leave at framework default. Lower TX power means lower PA current peaks,
   which a marginal 3.3 V rail can actually hold up. */
/* 0 = framework default (19.5 dBm). Capping TX power was tested as a fix for the
   mid-download disconnects and made no difference — the cause was the service-AP
   timeout re-running WiFi.begin(), not RF brownout. Left configurable but disabled,
   since lowering it costs range for no demonstrated benefit. */
#define WIFI_TX_POWER_LIMIT         0                       ///< see wifi_power_t: 78=19.5dBm 68=17 60=15 52=13 44=11
#define WIFI_AUTO_RECONNECT_GRACE   3                       ///< WiFiReconnect() polls to wait for the stack's own auto-reconnect before forcing a disconnect/reconnect cycle
#define WIFI_RECOVER_RECONNECT_MS   60000                   ///< disconnect + reconnect after this long without STA [ms]
#define WIFI_RECOVER_RESTACK_MS     180000                  ///< full esp_wifi stop/start after this long [ms]
#define WIFI_RECOVER_REBOOT_MS      600000                  ///< reboot MCU after this long [ms]
#define WIFI_DISABLE_UNENCRYPTED_STA_PASS_CHECK false       ///< enable/disable WEP/WPA/WPA2/... encryption for STA mode . for the wifi network without encryption set to false

/* ----------------- NTP CFG --------------------*/
#define NTP_SERVER_1                "pool.ntp.org"          ///< NTP server
#define NTP_SERVER_2                "time.nist.gov"         ///< NTP server
#define NTP_GTM_OFFSET_SEC          0                       ///< GMT offset in seconds. 0 = UTC. 3600 = GMT+1
#define NTP_DAYLIGHT_OFFSET_SEC     0                       ///< daylight offset in seconds. 0 = no daylight saving time. 3600 = +1 hour

/* ------------------ EXIF CFG ------------------*/
#define CAMERA_MAKE                 "OmniVision"            ///< Camera make string
#define CAMERA_MODEL                "OV2640"                ///< Camera model string
#define CAMERA_SOFTWARE             "Prusa ESP32-cam"       ///< Camera software string
#define CAMERA_EXIF_ROTATION_STREAM false                   ///< enable camera exif rotation for stream

/* ---------------- TIMELAPS CFG ----------------*/
#define TIMELAPS_PHOTO_FOLDER       "/timelapse"            ///< folder for timelaps photos
#define TIMELAPS_PHOTO_PREFIX       "photo"                 ///< photo name for timelaps
#define TIMELAPS_PHOTO_SUFFIX       ".jpg"                  ///< photo file type for timelaps

/* ---------------- TIMELAPSE AVI CFG -----------*/
#define TIMELAPS_AVI_FOLDER         "/timelapse"            ///< folder for AVI timelapse files
#define TIMELAPS_AVI_PREFIX         "tl_"                   ///< AVI filename prefix
#define TIMELAPS_AVI_SUFFIX         ".avi"                  ///< AVI filename suffix
#define TIMELAPS_AVI_FPS            10                      ///< AVI playback framerate (not capture rate)
#define TIMELAPS_AVI_SEGMENT_FRAMES 100                     ///< frames per AVI segment before auto-split
/* Start a deferred recording anyway after this long. Without it an offline camera —
   where NTP never syncs — could never record at all.
   90 s was required while NTP rode the 28 s TASK_WIFI tick: sync measured 34.3 s from
   boot, and one failed attempt pushed it past 60 s. With ServiceNtpSync() on its own
   NTP_RETRY_MS cadence sync now measures 6.0 s, so 60 s allows roughly four attempts
   and no longer risks a premature fallback to an uptime-based filename. */
#define TL_START_READY_TIMEOUT_MS   60000
#define TL_NAME_JOB_MAX_CHARS       32                      ///< max chars of the gcode name embedded in the AVI filename
#define TL_LIST_MAX_FILES           64                      ///< cap on files returned by /timelapse/list (bounds the sort buffer)
#define TL_DOWNLOAD_IDLE_MS         20000                   ///< a download with no chunk progress for this long is treated as dead [ms]
#define TL_DOWNLOAD_LOCK_WAIT_MS    15                      ///< how long a download chunk waits for the recorder's mutex before deferring [ms]

/* ------- LAYER-CHANGE TIMELAPSE TRIGGER (PrusaLink axis_z) -------
   A frame is emitted when Z rises by at least TL_LAYER_MIN_DELTA_MM *and* the new
   height is confirmed by two consecutive polls. The confirmation is what rejects
   Z-hop: a hop is a brief move during travel, so it is very unlikely to be sampled
   at the same height twice in a row, whereas a real layer holds its Z for the whole
   layer. TL_LAYER_POLL_MS must therefore be well under a layer's print time. */
#define TL_LAYER_MIN_DELTA_MM       0.04f                   ///< minimum Z rise to count as a new layer [mm]
#define TL_LAYER_MAX_JUMP_MM        5.0f                    ///< a confirmed Z rise larger than this is a park/lift, not a layer — ignored (observed 3.2 -> 168 at print end) [mm]
#define PL_AXIS_Z_MIN_MM            -5.0f                   ///< below this, axis_z is an unhomed/garbage reading (observed -412 while homing). Small negatives are valid: live-Z offset [mm]
#define PL_AXIS_Z_MAX_MM            2000.0f                 ///< above this, axis_z is garbage [mm]
#define TL_LAYER_Z_EPSILON_MM       0.01f                   ///< two Z readings within this are "the same height" [mm]
#define TL_LAYER_POLL_MS            5000                    ///< PrusaLink poll interval while printing [ms]
#define TL_IDLE_POLL_MS             30000                   ///< PrusaLink poll interval when not printing [ms]


/* ------------------ OTA UPDATE ------------------
   On-demand only — there is no background polling. See ota.h for why. */
#define OTA_API_HOST                "api.github.com"
#define OTA_API_PATH                "/repos/gw-tan/Prusa-ESP32-Cam-Enhanced/releases/latest"
/* OTA_ASSET_NAME is per board — see the OTA UPDATE CFG block in each
   src/module_<board>.h. It must not live here: one global name would hand every
   board the same image, and an app image built for another board is at best a
   camera with the wrong pins, at worst (ESP32-S3 app on an ESP32) a device that
   will not boot. A board whose asset is absent from the release is told exactly
   that by OtaUpdate::DoCheck, which is the correct outcome for a board this
   project does not publish binaries for. */
#define OTA_HTTP_TIMEOUT_MS         15000                           ///< per-request timeout [ms]
#define OTA_CHUNK_BYTES             1024                            ///< download buffer; stack-allocated, keep modest
#define OTA_STALL_TIMEOUT_MS        20000                           ///< abort if no bytes arrive for this long [ms]

/* ---------------- FACTORY CFG  ----------------*/
#define FACTORY_CFG_PHOTO_REFRESH_INTERVAL    30                ///< in the second
#define FACTORY_CFG_PHOTO_QUALITY             10                ///< 10-63, lower is better
#define FACTORY_CFG_FRAME_SIZE                0                 ///< 0 - FRAMESIZE_QVGA, ..., 6 - FRAMESIZE_UXGA. Look function Cfg_TransformFrameSizeDataType
#define FACTORY_CFG_BRIGHTNESS                0                 ///< from -2 to 2
#define FACTORY_CFG_CONTRAST                  0                 ///< from -2 to 2
#define FACTORY_CFG_SATURATION                0                 ///< from -2 to 2
#define FACTORY_CFG_H_MIRROR                  0                 ///< Horizontal mirror. 0 - false, 1 - true
#define FACTORY_CFG_V_FLIP                    0                 ///< Vertical flip. 0 - false, 1 - true
#define FACTORY_CFG_LENS_CORRECT              1                 ///< 0 - false, 1 - true
#define FACTORY_CFG_EXPOSURE_CTRL             1                 ///< 0 - false, 1 - true
#define FACTORY_CFG_AWB                       1                 ///< automatic white balancing 0 - false, 1 - true
#define FACTORY_CFG_AWB_GAIN                  1                 ///< automatic white balancing gain 0 - false, 1 - true
#define FACTORY_CFG_AWB_MODE                  0                 ///< automatic white balancing mode (0 - Auto, 1 - Sunny, 2 - Cloudy, 3 - Office, 4 - Home)
#define FACTORY_CFG_BPC                       1                 ///< bad pixel detection
#define FACTORY_CFG_WPC                       1                 ///< white pixel correction
#define FACTORY_CFG_RAW_GAMA                  1                 ///< raw gama
#define FACTORY_CFG_WEB_AUTH_USERNAME         F("admin")        ///< user name for login to WEB interface. definition WEB_ENABLE_BASIC_AUTH must be true
#define FACTORY_CFG_WEB_AUTH_PASSWORD         F("admin")        ///< password for login to WEB interface. definition WEB_ENABLE_BASIC_AUTH must be true
#define FACTORY_CFG_WEB_AUTH_ENABLE           false             ///< enable web auth for login to WEB interface. definition WEB_ENABLE_BASIC_AUTH must be 
#define FACTORY_CFG_CAMERA_FLASH_ENABLE       false             ///< enable camera flash functionality
#define FACTORY_CFG_CAMERA_FLASH_TIME         200               ///< time for camera flash duration time [ms]
#define FACTORY_CFG_MDNS_RECORD_HOST          F("prusa-esp32cam") ///< mdns record http://MDNS_RECORD_HOST.local
#define FACTORY_CFG_AEC2                      0                 ///< enable automatic exposition
#define FACTORY_CFG_AE_LEVEL                  0                 ///< automatic exposition level
#define FACTORY_CFG_AEC_VALUE                 300               ///< automatic exposition time
#define FACTORY_CFG_GAIN_CTRL                 1                 ///< enable automatic gain
#define FACTORY_CFG_AGC_GAIN                  0                 ///< automatic gain controll gain
#define FACTORY_CFG_HOSTNAME                  F("connect.prusa3d.com")  ///< hostname for Prusa Connect
#define FACTORY_CFG_ENABLE_SERVICE_AP         1                 ///< enable service AP mode
#define FACTORY_CFG_NETWORK_IP_METHOD         0                 ///< 0 - DHCP, 1 - Static IP
#define FACTORY_CFG_NETWORK_STATIC_IP         F("255.255.255.255") ///< Static IP address
#define FACTORY_CFG_NETWORK_STATIC_MASK       F("255.255.255.255") ///< Static Mask
#define FACTORY_CFG_NETWORK_STATIC_GATEWAY    F("255.255.255.255") ///< Static Gateway
#define FACTORY_CFG_NETWORK_STATIC_DNS        F("255.255.255.255") ///< Static DNS
#define FACTORY_CFG_IMAGE_EXIF_ROTATION       1                 ///< Image rotation 1 - 0°, 6 - 90°, 3 - 180°, 8 - 270°
#define FACTORY_CFG_TIMELAPS_ENABLE           0                 ///< enable timelaps functionality
#define FACTORY_CFG_UPLOAD_ENABLE             1                 ///< 1 = upload to Prusa Connect enabled
#define FACTORY_CFG_PL_STOP_UPLOAD_ON_DONE    0                 ///< 0 = keep uploading after print ends (default)

/* ---------------- CFG FLAGS  ------------------*/
#define CFG_WIFI_SETTINGS_SAVED               0x0A              ///< flag saved config
#define CFG_WIFI_SETTINGS_NOT_SAVED           0xFF              ///< flag notsaved config
#define CFG_FIRST_MCU_START_ACK               0xFF              ///< flag first MCU start ACK -> yes, it's first mcu start
#define CFG_FIRST_MCU_START_NAK               0x0F              ///< flag first MCU start NAK -> no, it's not first MCU start
#define SECOND_TO_MILISECOND                  1000              ///< constant for convert ms to second

/* ---------------- EEPROM CFG ------------------*/
#define EEPROM_ADDR_REFRESH_INTERVAL_START        0   ///< whre is stored first byte from refresh data
#define EEPROM_ADDR_REFRESH_INTERVAL_LENGTH       1   ///< how long is the refresh data variable stored in the eeprom [bytes]

#define EEPROM_ADDR_FINGERPRINT_START             (EEPROM_ADDR_REFRESH_INTERVAL_START + EEPROM_ADDR_REFRESH_INTERVAL_LENGTH)    ///< where is stored first byte from refresh interval
#define EEPROM_ADDR_FINGERPRINT_LENGTH            80                                                                            ///< how long is refresh interval [bytes]

#define EEPROM_ADDR_TOKEN_START                   (EEPROM_ADDR_FINGERPRINT_START + EEPROM_ADDR_FINGERPRINT_LENGTH)              ///< where is stored first byte from fingerprint
#define EEPROM_ADDR_TOKEN_LENGTH                  40                                                                            ///< how long is fingerprint [bytes]

#define EEPROM_ADDR_FRAMESIZE_START               (EEPROM_ADDR_TOKEN_START + EEPROM_ADDR_TOKEN_LENGTH)                          ///< where is stored token
#define EEPROM_ADDR_FRAMESIZE_LENGTH              1                                                                             ///< how long is token 

#define EEPROM_ADDR_BRIGHTNESS_START              (EEPROM_ADDR_FRAMESIZE_START + EEPROM_ADDR_FRAMESIZE_LENGTH)                  ///< where is stored framesize
#define EEPROM_ADDR_BRIGHTNESS_LENGTH             1                                                                             ///< how long is framesize

#define EEPROM_ADDR_CONTRAST_START                (EEPROM_ADDR_BRIGHTNESS_START + EEPROM_ADDR_BRIGHTNESS_LENGTH)                ///< where is stored brightness
#define EEPROM_ADDR_CONTRAST_LENGTH               1                                                                             ///< how long is brightness

#define EEPROM_ADDR_SATURATION_START              (EEPROM_ADDR_CONTRAST_START + EEPROM_ADDR_CONTRAST_LENGTH)                    ///< where is stored contrast
#define EEPROM_ADDR_SATURATION_LENGTH             1                                                                             ///< how long is contrast

#define EEPROM_ADDR_HMIRROR_START                 (EEPROM_ADDR_SATURATION_START + EEPROM_ADDR_SATURATION_LENGTH)                ///< where is stored saturation
#define EEPROM_ADDR_HMIRROR_LENGTH                1                                                                             ///< how long is saturation

#define EEPROM_ADDR_VFLIP_START                   (EEPROM_ADDR_HMIRROR_START + EEPROM_ADDR_HMIRROR_LENGTH)                      ///< where is stored hmirror
#define EEPROM_ADDR_VFLIP_LENGTH                  1                                                                             ///< how long is hmirror

#define EEPROM_ADDR_LENSC_START                   (EEPROM_ADDR_VFLIP_START + EEPROM_ADDR_VFLIP_LENGTH)                          ///< where is stored vflip
#define EEPROM_ADDR_LENSC_LENGTH                  1                                                                             ///< how long is vflip

#define EEPROM_ADDR_EXPOSURE_CTRL_START           (EEPROM_ADDR_LENSC_START + EEPROM_ADDR_LENSC_LENGTH)                          ///< where is stored lens correction
#define EEPROM_ADDR_EXPOSURE_CTRL_LENGTH          1                                                                             ///< how long is lens correction

#define EEPROM_ADDR_PHOTO_QUALITY_START           (EEPROM_ADDR_EXPOSURE_CTRL_START + EEPROM_ADDR_EXPOSURE_CTRL_LENGTH)          ///< where is stored exposure ctrl
#define EEPROM_ADDR_PHOTO_QUALITY_LENGTH          1                                                                             ///< how long is exposure ctrl

#define EEPROM_ADDR_WIFI_SSID_START               (EEPROM_ADDR_PHOTO_QUALITY_START + EEPROM_ADDR_PHOTO_QUALITY_LENGTH)          ///< where is stored wi-fi ssid
#define EEPROM_ADDR_WIFI_SSID_LENGTH              33                                                                            ///< maximum length for IEEE 802.11 is 32 + 1 for save ssid length

#define EEPROM_ADDR_WIFI_PASSWORD_START           (EEPROM_ADDR_WIFI_SSID_START + EEPROM_ADDR_WIFI_SSID_LENGTH)                  ///< where is stored wifi password 
#define EEPROM_ADDR_WIFI_PASSWORD_LENGTH          64                                                                            ///< maximum length for IEEE 802.11 is 63 + 1 for save password length

#define EEPROM_ADDR_WIFI_ACTIVE_FLAG_START        (EEPROM_ADDR_WIFI_PASSWORD_START + EEPROM_ADDR_WIFI_PASSWORD_LENGTH)          ///< where is stored information about stored cfg
#define EEPROM_ADDR_WIFI_ACTIVE_FLAG_LENGTH       1                                                                             ///< maximum lenght for cfg flag

#define EEPROM_ADDR_BASIC_AUTH_USERNAME_START     (EEPROM_ADDR_WIFI_ACTIVE_FLAG_START + EEPROM_ADDR_WIFI_ACTIVE_FLAG_LENGTH)    ///< where is stored username for login with basic auth. 
#define EEPROM_ADDR_BASIC_AUTH_USERNAME_LENGTH    11                                                                            ///< maximum length for username is 10 byte + 1 byte for save length

#define EEPROM_ADDR_BASIC_AUTH_PASSWORD_START     (EEPROM_ADDR_BASIC_AUTH_USERNAME_START + EEPROM_ADDR_BASIC_AUTH_USERNAME_LENGTH)        ///< where is stored password for login with basic auth
#define EEPROM_ADDR_BASIC_AUTH_PASSWORD_LENGTH    21                                                                                      ///< maximum length for password is 20 byte + 1 byte for save length

#define EEPROM_ADDR_BASIC_AUTH_ENABLE_FLAG_START  (EEPROM_ADDR_BASIC_AUTH_PASSWORD_START + EEPROM_ADDR_BASIC_AUTH_PASSWORD_LENGTH)        ///< where is stored flag for enable/disable basic auth from user
#define EEPROM_ADDR_BASIC_AUTH_ENABLE_FLAG_LENGTH 1                                                                                       ///< how long is flag

#define EEPROM_ADDR_FIRST_MCU_START_FLAG_START    (EEPROM_ADDR_BASIC_AUTH_ENABLE_FLAG_START + EEPROM_ADDR_BASIC_AUTH_ENABLE_FLAG_LENGTH)  ///< where is stored flag for first MCU start check
#define EEPROM_ADDR_FIRST_MCU_START_FLAG_LENGTH   1                                                                                       ///< how long is flag

#define EEPROM_ADDR_CAMERA_FLASH_ENABLE_START     (EEPROM_ADDR_FIRST_MCU_START_FLAG_START + EEPROM_ADDR_FIRST_MCU_START_FLAG_LENGTH)      ///< where is stored flag for enable/disable camera flash
#define EEPROM_ADDR_CAMERA_FLASH_ENABLE_LENGTH    1                                                                                       ///< how long is flag

#define EEPROM_ADDR_CAMERA_FLASH_TIME_START       (EEPROM_ADDR_CAMERA_FLASH_ENABLE_START + EEPROM_ADDR_CAMERA_FLASH_ENABLE_LENGTH)        ///< where is stored value camera flash during time
#define EEPROM_ADDR_CAMERA_FLASH_TIME_LENGTH      2                                                                                       ///< how long is the value

#define EEPROM_ADDR_MDNS_RECORD_START             (EEPROM_ADDR_CAMERA_FLASH_TIME_START + EEPROM_ADDR_CAMERA_FLASH_TIME_LENGTH)
#define EEPROM_ADDR_MDNS_RECORD_LENGTH            41

#define EEPROM_ADDR_AWB_ENABLE_START              (EEPROM_ADDR_MDNS_RECORD_START + EEPROM_ADDR_MDNS_RECORD_LENGTH)
#define EEPROM_ADDR_AWB_ENABLE_LENGTH             1

#define EEPROM_ADDR_AWB_GAIN_ENABLE_START         (EEPROM_ADDR_AWB_ENABLE_START + EEPROM_ADDR_AWB_ENABLE_LENGTH)
#define EEPROM_ADDR_AWB_GAIN_ENABLE_LENGTH        1

#define EEPROM_ADDR_AWB_MODE_ENABLE_START         (EEPROM_ADDR_AWB_GAIN_ENABLE_START + EEPROM_ADDR_AWB_GAIN_ENABLE_LENGTH)
#define EEPROM_ADDR_AWB_MODE_ENABLE_LENGTH        1

#define EEPROM_ADDR_BPC_ENABLE_START              (EEPROM_ADDR_AWB_MODE_ENABLE_START + EEPROM_ADDR_AWB_MODE_ENABLE_LENGTH)
#define EEPROM_ADDR_BPC_ENABLE_LENGTH             1

#define EEPROM_ADDR_WPC_ENABLE_START              (EEPROM_ADDR_BPC_ENABLE_START + EEPROM_ADDR_BPC_ENABLE_LENGTH)
#define EEPROM_ADDR_WPC_ENABLE_LENGTH             1

#define EEPROM_ADDR_RAW_GAMA_ENABLE_START         (EEPROM_ADDR_WPC_ENABLE_START + EEPROM_ADDR_WPC_ENABLE_LENGTH)
#define EEPROM_ADDR_RAW_GAMA_ENABLE_LENGTH        1

#define EEPROM_ADDR_AEC2_START                    (EEPROM_ADDR_RAW_GAMA_ENABLE_START + EEPROM_ADDR_RAW_GAMA_ENABLE_LENGTH)
#define EEPROM_ADDR_AEC2_LENGTH                   1

#define EEPROM_ADDR_AE_LEVEL_START                (EEPROM_ADDR_AEC2_START + EEPROM_ADDR_AEC2_LENGTH)
#define EEPROM_ADDR_AE_LEVEL_LENGTH               1

#define EEPROM_ADDR_AEC_VALUE_START               (EEPROM_ADDR_AE_LEVEL_START + EEPROM_ADDR_AE_LEVEL_LENGTH)   
#define EEPROM_ADDR_AEC_VALUE_LENGTH              2

#define EEPROM_ADDR_GAIN_CTRL_START               (EEPROM_ADDR_AEC_VALUE_START + EEPROM_ADDR_AEC_VALUE_LENGTH)
#define EEPROM_ADDR_GAIN_CTRL_LENGTH              1

#define EEPROM_ADDR_AGC_GAIN_START                (EEPROM_ADDR_GAIN_CTRL_START + EEPROM_ADDR_GAIN_CTRL_LENGTH)
#define EEPROM_ADDR_AGC_GAIN_LENGTH               1

#define EEPROM_ADDR_LOG_LEVEL                     (EEPROM_ADDR_AGC_GAIN_START + EEPROM_ADDR_AGC_GAIN_LENGTH)
#define EEPROM_ADDR_LOG_LEVEL_LENGTH              1

#define EEPROM_ADDR_HOSTNAME_START                (EEPROM_ADDR_LOG_LEVEL + EEPROM_ADDR_LOG_LEVEL_LENGTH)
#define EEPROM_ADDR_HOSTNAME_LENGTH               51

#define EEPROM_ADDR_SERVICE_AP_ENABLE_START       (EEPROM_ADDR_HOSTNAME_START + EEPROM_ADDR_HOSTNAME_LENGTH)
#define EEPROM_ADDR_SERVICE_AP_ENABLE_LENGTH      1

#define EEPROM_ADDR_NETWORK_IP_METHOD_START       (EEPROM_ADDR_SERVICE_AP_ENABLE_START + EEPROM_ADDR_SERVICE_AP_ENABLE_LENGTH)
#define EEPROM_ADDR_NETWORK_IP_METHOD_LENGTH      1

#define EEPROM_ADDR_NETWORK_STATIC_IP_START       (EEPROM_ADDR_NETWORK_IP_METHOD_START + EEPROM_ADDR_NETWORK_IP_METHOD_LENGTH)
#define EEPROM_ADDR_NETWORK_STATIC_IP_LENGTH      4

#define EEPROM_ADDR_NETWORK_STATIC_MASK_START     (EEPROM_ADDR_NETWORK_STATIC_IP_START + EEPROM_ADDR_NETWORK_STATIC_IP_LENGTH)
#define EEPROM_ADDR_NETWORK_STATIC_MASK_LENGTH    4

#define EEPROM_ADDR_NETWORK_STATIC_GATEWAY_START  (EEPROM_ADDR_NETWORK_STATIC_MASK_START + EEPROM_ADDR_NETWORK_STATIC_MASK_LENGTH)
#define EEPROM_ADDR_NETWORK_STATIC_GATEWAY_LENGTH 4

#define EEPROM_ADDR_NETWORK_STATIC_DNS_START      (EEPROM_ADDR_NETWORK_STATIC_GATEWAY_START + EEPROM_ADDR_NETWORK_STATIC_GATEWAY_LENGTH)
#define EEPROM_ADDR_NETWORK_STATIC_DNS_LENGTH     4

#define EEPROM_ADDR_IMAGE_ROTATION_START          (EEPROM_ADDR_NETWORK_STATIC_DNS_START + EEPROM_ADDR_NETWORK_STATIC_DNS_LENGTH)
#define EEPROM_ADDR_IMAGE_ROTATION_LENGTH         1

#define EEPROM_ADDR_TIMELAPS_ENABLE_START         (EEPROM_ADDR_IMAGE_ROTATION_START + EEPROM_ADDR_IMAGE_ROTATION_LENGTH)
#define EEPROM_ADDR_TIMELAPS_ENABLE_LENGTH        1

/* Two bytes that held the external temperature sensor's enable flag and unit.
   The sensor is gone, but this slot must stay: every address below is derived
   from it, so reclaiming the gap would move an already-provisioned device's
   PrusaLink IP and API key and read them back as garbage after an app-only
   upgrade. Reuse it for the next setting that needs a byte or two. */
#define EEPROM_ADDR_RESERVED_2B_START             (EEPROM_ADDR_TIMELAPS_ENABLE_START + EEPROM_ADDR_TIMELAPS_ENABLE_LENGTH)
#define EEPROM_ADDR_RESERVED_2B_LENGTH            2

/* ---------------- PRUSA LINK CFG  ----------------*/
#define EEPROM_ADDR_PRUSA_LINK_ENABLE_START       (EEPROM_ADDR_RESERVED_2B_START + EEPROM_ADDR_RESERVED_2B_LENGTH)
#define EEPROM_ADDR_PRUSA_LINK_ENABLE_LENGTH      1

#define EEPROM_ADDR_PRUSA_LINK_IP_START           (EEPROM_ADDR_PRUSA_LINK_ENABLE_START + EEPROM_ADDR_PRUSA_LINK_ENABLE_LENGTH)
#define EEPROM_ADDR_PRUSA_LINK_IP_LENGTH          16   ///< "255.255.255.255\0"

#define EEPROM_ADDR_PRUSA_LINK_API_KEY_START      (EEPROM_ADDR_PRUSA_LINK_IP_START + EEPROM_ADDR_PRUSA_LINK_IP_LENGTH)
#define EEPROM_ADDR_PRUSA_LINK_API_KEY_LENGTH     33   ///< 32-char key + 1 length byte

#define EEPROM_ADDR_TL_TRIGGER_MODE_START         (EEPROM_ADDR_PRUSA_LINK_API_KEY_START + EEPROM_ADDR_PRUSA_LINK_API_KEY_LENGTH)
#define EEPROM_ADDR_TL_TRIGGER_MODE_LENGTH        1    ///< timelapse trigger mode (0=manual, 1=printer)

#define EEPROM_ADDR_TL_WAS_RECORDING_START        (EEPROM_ADDR_TL_TRIGGER_MODE_START + EEPROM_ADDR_TL_TRIGGER_MODE_LENGTH)
#define EEPROM_ADDR_TL_WAS_RECORDING_LENGTH       1    ///< 1 if timelapse was recording at last shutdown

#define EEPROM_ADDR_TL_SEGMENT_COUNT_START        (EEPROM_ADDR_TL_WAS_RECORDING_START + EEPROM_ADDR_TL_WAS_RECORDING_LENGTH)
#define EEPROM_ADDR_TL_SEGMENT_COUNT_LENGTH       2    ///< uint16_t: segment counter for resume after restart

#define EEPROM_ADDR_TL_MAX_FRAMES_START           (EEPROM_ADDR_TL_SEGMENT_COUNT_START + EEPROM_ADDR_TL_SEGMENT_COUNT_LENGTH)
#define EEPROM_ADDR_TL_MAX_FRAMES_LENGTH          2    ///< uint16_t: max frames per segment (0 = unlimited)

#define EEPROM_ADDR_UPLOAD_ENABLE_START           (EEPROM_ADDR_TL_MAX_FRAMES_START + EEPROM_ADDR_TL_MAX_FRAMES_LENGTH)
#define EEPROM_ADDR_UPLOAD_ENABLE_LENGTH          1    ///< 1 = upload to Prusa Connect enabled, 0 = disabled

#define EEPROM_ADDR_PL_STOP_UPLOAD_ON_DONE_START  (EEPROM_ADDR_UPLOAD_ENABLE_START + EEPROM_ADDR_UPLOAD_ENABLE_LENGTH)
#define EEPROM_ADDR_PL_STOP_UPLOAD_ON_DONE_LENGTH 1    ///< 1 = auto-disable upload when print finishes

#define EEPROM_ADDR_TL_FRAME_MODE_START           (EEPROM_ADDR_PL_STOP_UPLOAD_ON_DONE_START + EEPROM_ADDR_PL_STOP_UPLOAD_ON_DONE_LENGTH)
#define EEPROM_ADDR_TL_FRAME_MODE_LENGTH          1    ///< timelapse frame trigger: 0 = time-based, 1 = per layer (PrusaLink axis_z)

/* Derived from the last field's start + length so it stays correct automatically
   when new fields are appended to the address chain. */
#define EEPROM_SIZE (EEPROM_ADDR_TL_FRAME_MODE_START + EEPROM_ADDR_TL_FRAME_MODE_LENGTH)

#endif

/* EOF */