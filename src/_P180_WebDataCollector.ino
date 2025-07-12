#include "_Plugin_Helper.h"
#ifdef USES_P180

// #######################################################################################################
// #################################### Plugin-180: Web Data Collector ################################
// #######################################################################################################

#define PLUGIN_180
#define PLUGIN_ID_180 180
#define PLUGIN_NAME_180 "Web Data Collector"
#define PLUGIN_VALUENAME1_180 "Status"

#ifdef ESP8266
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <FS.h>
#endif // ifdef ESP8266

#ifdef ESP32
#include "HTTPClient.h"
#include "SPIFFS.h"
#endif // ifdef ESP32

#include <ArduinoJson.h>
#include <WiFiClient.h>

// Global variables for this plugin
struct {
  unsigned long lastUpdate = 0;
  String baseUrl = "";
  String dataFilename = "";
  unsigned long updateInterval = 30000; // 30 seconds default
  bool autoUpdate = true;
}

// Enhanced function to check current guest with your specific logic
void Plugin_180_checkCurrentGuest(const String& address, const String& door, const String& tag) {
  String log;
  bool found = false;
  
  if (loglevelActiveFor(LOG_LEVEL_INFO)) {
    log = F("WebCollector: Checking tag ");
    log += tag;
    log += F(" for door ");
    log += door;
    addLog(LOG_LEVEL_INFO, log);
  }
  
  // First check local storage
  String currentGuests = Plugin_180_readFile(Plugin_180_Data.dataFilename); currentGuests = Plugin_099_readFile(Plugin_099_Data.dataFilename);
  
  if (currentGuests.length() > 10) {
    // Parse local JSON data
    #ifdef ESP8266
    DynamicJsonDocument doc(2048);
    #endif
    #ifdef ESP32
    DynamicJsonDocument doc(ESP.getMaxAllocHeap());
    #endif
    
    DeserializationError error = deserializeJson(doc, currentGuests);
    if (!error) {
      JsonArray array = doc.as<JsonArray>();
      
      // Check each guest in local storage
      for (JsonVariant v : array) {
        // Check for exact match (tag and door)
        if (v["tag"].as<String>() == tag && v["door"].as<String>() == door) {
          const char *first_name = v["first_name"];
          String log = F("WebCollector: Found guest in local storage - ");
          log += first_name;
          if (loglevelActiveFor(LOG_LEVEL_INFO)) {
            addLog(LOG_LEVEL_INFO, log);
          }
          
          // Trigger success event with guest info
          String command = "Event,success=1,";
          command += tag;
          ExecuteCommand_all(EventValueSource::Enum::VALUE_SOURCE_SYSTEM, command.c_str());
          found = true;
          return;
        }
        
        // Check for admin access
        if (v["tag"].as<String>() == tag && v["admin"].as<bool>()) {
          const char *first_name = v["first_name"];
          String log = F("WebCollector: Found admin in local storage - ");
          log += first_name;
          if (loglevelActiveFor(LOG_LEVEL_INFO)) {
            addLog(LOG_LEVEL_INFO, log);
          }
          
          // Trigger admin success event
          const char *command = "Event,success=2,Admin";
          ExecuteCommand_all(EventValueSource::Enum::VALUE_SOURCE_SYSTEM, command);
          found = true;
          return;
        }
      }
    }
  }
  
  // If not found locally, try to update from web and check again
  if (!found) {
    String log = F("WebCollector: Guest not found locally, checking web...");
    if (loglevelActiveFor(LOG_LEVEL_INFO)) {
      addLog(LOG_LEVEL_INFO, log);
    }
    
    // Build URL for web request
    String webUrl = Plugin_180_Data.baseUrl;
    if (webUrl.length() == 0) {
      // Fallback to default URLs if not configured
      #ifdef ESP8266
      webUrl = "http://homation.us/api/";  // Use HTTP for ESP8266 by default
      #endif
      #ifdef ESP32  
      webUrl = "https://homation.us/api/"; // Use HTTPS for ESP32 by default
      #endif
    }
    
    // Ensure URL ends with /
    if (!webUrl.endsWith("/")) {
      webUrl += "/";
    }
    
    webUrl += address;
    webUrl += "/";
    webUrl += door;
    
    // Make web request
    HttpResponse response = Plugin_180_httpGETRequest(webUrl.c_str());
    
    if (response.isValid && response.data.length() > 10) {
      // Parse web response
      #ifdef ESP8266
      DynamicJsonDocument webDoc(2048);
      #endif
      #ifdef ESP32
      DynamicJsonDocument webDoc(ESP.getMaxAllocHeap());
      #endif
      
      DeserializationError webError = deserializeJson(webDoc, response.data);
      if (!webError) {
        JsonArray webArray = webDoc.as<JsonArray>();
        
        // Check web data for the tag
        for (JsonVariant v : webArray) {
          // Check for exact match (tag and door)
          if (v["tag"].as<String>() == tag && v["door"].as<String>() == door) {
            const char *first_name = v["first_name"];
            String log = F("WebCollector: Found guest on web - ");
            log += first_name;
            if (loglevelActiveFor(LOG_LEVEL_INFO)) {
              addLog(LOG_LEVEL_INFO, log);
            }
            
            // Update local storage with new data
            Plugin_180_writeFile(Plugin_180_Data.dataFilename, response.data);
            
            // Trigger success event
            String command = "Event,success=1,";
            command += tag;
            ExecuteCommand_all(EventValueSource::Enum::VALUE_SOURCE_SYSTEM, command.c_str());
            found = true;
            break;
          }
          
          // Check for admin access
          if (v["tag"].as<String>() == tag && v["admin"].as<bool>()) {
            const char *first_name = v["first_name"];
            String log = F("WebCollector: Found admin on web - ");
            log += first_name;
            if (loglevelActiveFor(LOG_LEVEL_INFO)) {
              addLog(LOG_LEVEL_INFO, log);
            }
            
            // Update local storage
            Plugin_180_writeFile(Plugin_180_Data.dataFilename, response.data);
            
            // Trigger admin success event
            const char *command = "Event,success=2,Admin";
            ExecuteCommand_all(EventValueSource::Enum::VALUE_SOURCE_SYSTEM, command);
            found = true;
            break;
          }
        }
        
        // If we got new data but didn't find the tag, still update local storage
        if (!found && currentGuests != response.data) {
          Plugin_180_writeFile(Plugin_180_Data.dataFilename, response.data);
          String log = F("WebCollector: Updated local data from web");
          if (loglevelActiveFor(LOG_LEVEL_INFO)) {
            addLog(LOG_LEVEL_INFO, log);
          }
        }
      }
    }
  }
  
  // If still not found, trigger not found event
  if (!found) {
    String log = F("WebCollector: Tag not found - ");
    log += tag;
    if (loglevelActiveFor(LOG_LEVEL_INFO)) {
      addLog(LOG_LEVEL_INFO, log);
    }
    
    // Trigger not found event
    const char *command = "Event,notfound=1";
    ExecuteCommand_all(EventValueSource::Enum::VALUE_SOURCE_SYSTEM, command);
  }
}

// Function to update currents data (like your original GetCurrents)
bool Plugin_180_getCurrents(const String& path) {
  String webUrl = Plugin_180_Data.baseUrl;
  if (webUrl.length() == 0) {
    // Fallback to default URLs if not configured
    #ifdef ESP8266
    webUrl = "http://homation.us/api/";  // Use HTTP for ESP8266 by default
    #endif
    #ifdef ESP32
    webUrl = "https://homation.us/api/"; // Use HTTPS for ESP32 by default
    #endif
  }
  
  // Ensure URL ends with /
  if (!webUrl.endsWith("/")) {
    webUrl += "/";
  }
  
  webUrl += path;
  
  // Make HTTP request
  HttpResponse response = Plugin_180_httpGETRequest(webUrl.c_str());
  
  if (response.isValid && response.data.length() > 10) {
    // Read current local data
    String localData = Plugin_180_readFile(Plugin_180_Data.dataFilename);
    
    // Compare and update if different
    if (localData != response.data) {
      if (Plugin_180_writeFile(Plugin_180_Data.dataFilename, response.data)) {
        String log = F("WebCollector: Updated currents data from web");
        if (loglevelActiveFor(LOG_LEVEL_INFO)) {
          addLog(LOG_LEVEL_INFO, log);
        }
        return true;
      }
    } else {
      String log = F("WebCollector: Currents data unchanged");
      if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
        addLog(LOG_LEVEL_DEBUG, log);
      }
    }
  }
  
  return false; Plugin_099_Data;

// HTTP client instances
#ifdef ESP8266
HTTPClient http_099;
WiFiClient client_099;
#endif // ifdef ESP8266

#ifdef ESP32
HTTPClient http_099;
#endif // ifdef ESP32

// Function to make HTTP GET request and validate response
struct HttpResponse {
  String data;
  bool isValid;
  int statusCode;
};

HttpResponse Plugin_180_httpGETRequest(const char *serverName) {
  HttpResponse response;
  response.data = "";
  response.isValid = false;
  response.statusCode = 0;
  
  try {
    #ifdef ESP8266
    std::string s = serverName;
    
    // For HTTPS on ESP8266, use WiFiClientSecure
    if (strncmp(serverName, "https://", 8) == 0) {
      WiFiClientSecure secureClient;
      secureClient.setInsecure(); // Skip certificate verification for simplicity
      http_180.begin(secureClient, s.c_str());
    } else {
      http_180.begin(client_180, s.c_str());
    }
    #endif // ifdef ESP8266
    
    #ifdef ESP32
    http_180.begin(serverName);
    // ESP32 handles HTTPS automatically
    #endif // ifdef ESP32
    
    // Set timeout - longer for HTTPS
    bool isHttps = (strncmp(serverName, "https://", 8) == 0);
    http_180.setTimeout(isHttps ? 15000 : 10000); // 15s for HTTPS, 10s for HTTP
    
    // Add headers for better compatibility
    http_180.addHeader("User-Agent", "ESPEasy-WebCollector/1.0");
    http_180.addHeader("Accept", "application/json,text/plain,*/*");
    
    // Send HTTP GET request
    int httpResponseCode = http_180.GET();
    response.statusCode = httpResponseCode;
    
    if (httpResponseCode >= 200 && httpResponseCode < 300) {
      // Success response codes (2xx)
      response.data = http_180.getString();
      response.isValid = Plugin_180_validateData(response.data);
      
      String log = F("WebCollector: HTTP");
      log += isHttps ? F("S") : F("");
      log += F(" Success code: ");
      log += httpResponseCode;
      log += F(", Data valid: ");
      log += response.isValid ? F("Yes") : F("No");
      if (loglevelActiveFor(LOG_LEVEL_INFO)) {
        addLog(LOG_LEVEL_INFO, log);
      }
    } else if (httpResponseCode >= 400) {
      // Client/Server error codes (4xx, 5xx)
      String errorContent = http_180.getString();
      String log = F("WebCollector: HTTP");
      log += isHttps ? F("S") : F("");
      log += F(" Error code: ");
      log += httpResponseCode;
      log += F(" - Keeping existing data");
      if (loglevelActiveFor(LOG_LEVEL_ERROR)) {
        addLog(LOG_LEVEL_ERROR, log);
      }
      
      // Log error content for debugging (first 100 chars)
      if (errorContent.length() > 0 && loglevelActiveFor(LOG_LEVEL_DEBUG)) {
        String errorLog = F("WebCollector: Error content: ");
        errorLog += errorContent.substring(0, min(100, (int)errorContent.length()));
        addLog(LOG_LEVEL_DEBUG, errorLog);
      }
    } else {
      // Other error codes (1xx, 3xx, negative)
      String log = F("WebCollector: HTTP");
      log += isHttps ? F("S") : F("");
      log += F(" Unexpected code: ");
      log += httpResponseCode;
      if (loglevelActiveFor(LOG_LEVEL_ERROR)) {
        addLog(LOG_LEVEL_ERROR, log);
      }
    }
  } catch (...) {
    String log = F("WebCollector: HTTP");
    log += (strncmp(serverName, "https://", 8) == 0) ? F("S") : F("");
    log += F(" request exception");
    if (loglevelActiveFor(LOG_LEVEL_ERROR)) {
      addLog(LOG_LEVEL_ERROR, log);
    }
  }
  
  // Free resources
  http_180.end();
  return response;
}

// Function to validate if the received data is legitimate
bool Plugin_180_validateData(const String& data) {
  // Basic validation checks
  
  // Check minimum length
  if (data.length() < 10) {
    if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
      addLog(LOG_LEVEL_DEBUG, F("WebCollector: Data too short"));
    }
    return false;
  }
  
  // Check for common error patterns
  String lowerData = data;
  lowerData.toLowerCase();
  
  // Check for HTML error pages
  if (lowerData.indexOf(F("<html")) >= 0 || 
      lowerData.indexOf(F("<!doctype")) >= 0) {
    if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
      addLog(LOG_LEVEL_DEBUG, F("WebCollector: Received HTML instead of data"));
    }
    return false;
  }
  
  // Check for common error messages
  if (lowerData.indexOf(F("error")) >= 0 || 
      lowerData.indexOf(F("not found")) >= 0 ||
      lowerData.indexOf(F("internal server error")) >= 0 ||
      lowerData.indexOf(F("bad request")) >= 0 ||
      lowerData.indexOf(F("unauthorized")) >= 0 ||
      lowerData.indexOf(F("forbidden")) >= 0) {
    if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
      addLog(LOG_LEVEL_DEBUG, F("WebCollector: Data contains error messages"));
    }
    return false;
  }
  
  // If expecting JSON, validate JSON structure
  if (data.startsWith("{") || data.startsWith("[")) {
    DynamicJsonDocument testDoc(1024);
    DeserializationError error = deserializeJson(testDoc, data);
    if (error) {
      String log = F("WebCollector: Invalid JSON - ");
      log += error.c_str();
      if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
        addLog(LOG_LEVEL_DEBUG, log);
      }
      return false;
    }
  }
  
  // All checks passed
  return true;
}

// Function to read file from filesystem
String Plugin_180_readFile(const String& filename) {
  String content = "";
  
  #ifdef ESP8266
  if (SPIFFS.exists(filename)) {
    File file = SPIFFS.open(filename, "r");
    if (file) {
      while (file.available()) {
        content += char(file.read());
      }
      file.close();
    }
  }
  #endif // ifdef ESP8266
  
  #ifdef ESP32
  if (SPIFFS.exists(filename)) {
    File file = SPIFFS.open(filename, "r");
    if (file) {
      while (file.available()) {
        content += char(file.read());
      }
      file.close();
    }
  }
  #endif // ifdef ESP32
  
  return content;
}

// Function to write file to filesystem
bool Plugin_180_writeFile(const String& filename, const String& content) {
  #ifdef ESP8266
  File file = SPIFFS.open(filename, "w");
  #endif // ifdef ESP8266
  
  #ifdef ESP32
  File file = SPIFFS.open(filename, "w");
  #endif // ifdef ESP32
  
  if (file) {
    file.print(content);
    file.close();
    return true;
  }
  
  return false;
}

// Function to compare and update data
bool Plugin_180_updateData() {
  if (Plugin_180_Data.baseUrl.length() == 0) {
    if (loglevelActiveFor(LOG_LEVEL_ERROR)) {
      addLog(LOG_LEVEL_ERROR, F("WebCollector: No URL configured"));
    }
    return false;
  }
  
  // Make HTTP request and validate response
  HttpResponse response = Plugin_180_httpGETRequest(Plugin_180_Data.baseUrl.c_str());
  
  // Only proceed if we got valid data
  if (!response.isValid) {
    String log = F("WebCollector: Invalid/error response received - keeping existing data");
    if (loglevelActiveFor(LOG_LEVEL_INFO)) {
      addLog(LOG_LEVEL_INFO, log);
    }
    return false;
  }
  
  // Read current file data
  String fileData = Plugin_180_readFile(Plugin_180_Data.dataFilename);
  
  // Compare data - only update if different AND valid
  if (fileData != response.data) {
    // Data is different and valid, update file
    if (Plugin_180_writeFile(Plugin_180_Data.dataFilename, response.data)) {
      String log = F("WebCollector: Data updated from web (");
      log += response.data.length();
      log += F(" bytes)");
      if (loglevelActiveFor(LOG_LEVEL_INFO)) {
        addLog(LOG_LEVEL_INFO, log);
      }
      return true;
    } else {
      if (loglevelActiveFor(LOG_LEVEL_ERROR)) {
        addLog(LOG_LEVEL_ERROR, F("WebCollector: Failed to write file"));
      }
      return false;
    }
  } else {
    String log = F("WebCollector: Data unchanged");
    if (loglevelActiveFor(LOG_LEVEL_DEBUG)) {
      addLog(LOG_LEVEL_DEBUG, log);
    }
    return false;
  }
}

// Main plugin function
boolean Plugin_180(byte function, struct EventStruct *event, String &string) {
  boolean success = false;

  switch (function) {
    case PLUGIN_DEVICE_ADD: {
      Device[++deviceCount].Number = PLUGIN_ID_180;
      Device[deviceCount].Type = DEVICE_TYPE_DUMMY;
      Device[deviceCount].VType = Sensor_VType::SENSOR_TYPE_SINGLE;
      Device[deviceCount].Ports = 0;
      Device[deviceCount].PullUpOption = false;
      Device[deviceCount].InverseLogicOption = false;
      Device[deviceCount].ValueCount = 1;
      Device[deviceCount].SendDataOption = true;
      Device[deviceCount].TimerOption = true;
      Device[deviceCount].GlobalSyncOption = false;
      break;
    }

    case PLUGIN_GET_DEVICENAME: {
      string = F(PLUGIN_NAME_180);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES: {
      strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_180));
      break;
    }

    case PLUGIN_WEBFORM_LOAD: {
      // Web URL input
      addFormTextBox(F("Base URL"), F("p180_baseurl"), PCONFIG_LABEL(0), 255);
      addFormNote(F("Complete URL to fetch data from (e.g., https://example.com/api/data)"));
      
      // Filename input
      addFormTextBox(F("Data Filename"), F("p180_filename"), PCONFIG_LABEL(1), 64);
      addFormNote(F("Filename to store data (e.g., /webdata.json)"));
      
      // Update interval
      addFormNumericBox(F("Update Interval"), F("p180_interval"), PCONFIG_LONG(0), 5, 3600);
      addUnit(F("seconds"));
      addFormNote(F("How often to check for updates (5-3600 seconds)"));
      
      // Auto update checkbox
      addFormCheckBox(F("Auto Update"), F("p180_autoupdate"), PCONFIG(0));
      addFormNote(F("Automatically update data at specified interval"));
      
      // Manual update button
      addFormSeparator(2);
      addFormSubHeader(F("Manual Operations"));
      
      // Display current file content (first 500 chars)
      String fileContent = Plugin_180_readFile(Plugin_180_Data.dataFilename);
      if (fileContent.length() > 0) {
        addFormSubHeader(F("Current File Content"));
        String displayContent = fileContent;
        if (displayContent.length() > 500) {
          displayContent = displayContent.substring(0, 500) + "...";
        }
        addFormNote(displayContent);
      }
      
      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE: {
      // Save configuration
      strncpy_webserver_arg(PCONFIG_LABEL(0), F("p180_baseurl"));
      strncpy_webserver_arg(PCONFIG_LABEL(1), F("p180_filename"));
      PCONFIG_LONG(0) = getFormItemInt(F("p180_interval"));
      PCONFIG(0) = isFormItemChecked(F("p180_autoupdate"));
      
      // Update global data
      Plugin_180_Data.baseUrl = PCONFIG_LABEL(0);
      Plugin_180_Data.dataFilename = PCONFIG_LABEL(1);
      Plugin_180_Data.updateInterval = PCONFIG_LONG(0) * 1000; // Convert to milliseconds
      Plugin_180_Data.autoUpdate = PCONFIG(0);
      
      // Ensure minimum interval
      if (Plugin_180_Data.updateInterval < 5000) {
        Plugin_180_Data.updateInterval = 5000;
      }
      
      success = true;
      break;
    }

    case PLUGIN_INIT: {
      // Initialize plugin data
      Plugin_180_Data.baseUrl = PCONFIG_LABEL(0);
      Plugin_180_Data.dataFilename = PCONFIG_LABEL(1);
      Plugin_180_Data.updateInterval = PCONFIG_LONG(0) * 1000;
      Plugin_180_Data.autoUpdate = PCONFIG(0);
      
      // Ensure minimum interval
      if (Plugin_180_Data.updateInterval < 5000) {
        Plugin_180_Data.updateInterval = 5000;
      }
      
      // Initialize filesystem
      #ifdef ESP8266
      if (!SPIFFS.begin()) {
        addLog(LOG_LEVEL_ERROR, F("WebCollector: SPIFFS initialization failed"));
      }
      #endif // ifdef ESP8266
      
      #ifdef ESP32
      if (!SPIFFS.begin(true)) {
        addLog(LOG_LEVEL_ERROR, F("WebCollector: SPIFFS initialization failed"));
      }
      #endif // ifdef ESP32
      
      Plugin_180_Data.lastUpdate = 0; // Force immediate update
      success = true;
      break;
    }

    case PLUGIN_WRITE: {
      String cmd = parseString(string, 1);
      
      if (cmd.equalsIgnoreCase(F("WebCollector"))) {
        String subcmd = parseString(string, 2);
        
        if (subcmd.equalsIgnoreCase(F("Update"))) {
          // Manual update command
          bool updated = Plugin_180_updateData();
          UserVar.setFloat(event->TaskIndex, 0, updated ? 1.0 : 0.0);
          
          String log = F("WebCollector: Manual update ");
          log += updated ? F("successful") : F("completed (no changes or error)");
          if (loglevelActiveFor(LOG_LEVEL_INFO)) {
            addLog(LOG_LEVEL_INFO, log);
          }
          success = true;
        }
        else if (subcmd.equalsIgnoreCase(F("Read"))) {
          // Read file command
          String filename = parseStringKeepCase(string, 3);
          if (filename.length() > 0) {
            String content = Plugin_180_readFile(filename);
            
            String log = F("WebCollector: Read file ");
            log += filename;
            log += F(" - ");
            log += content.length();
            log += F(" bytes");
            if (loglevelActiveFor(LOG_LEVEL_INFO)) {
              addLog(LOG_LEVEL_INFO, log);
            }
          }
          success = true;
        }
        else if (subcmd.equalsIgnoreCase(F("SetURL"))) {
          // Set URL command
          String url = parseStringKeepCase(string, 3);
          if (url.length() > 0) {
            Plugin_180_Data.baseUrl = url;
            strncpy(PCONFIG_LABEL(0), url.c_str(), sizeof(PCONFIG_LABEL(0)) - 1);
            
            String log = F("WebCollector: URL set to ");
            log += url;
            if (loglevelActiveFor(LOG_LEVEL_INFO)) {
              addLog(LOG_LEVEL_INFO, log);
            }
          }
          success = true;
        }
      }
      
      // Enhanced GetCurrentGuest command with your logic
      if (cmd.equalsIgnoreCase(F("GetCurrentGuest"))) {
        String subcmd = parseString(string, 2);
        String address = parseStringKeepCase(string, 3);
        String door = parseStringKeepCase(string, 4);
        String tag = parseStringKeepCase(string, 5);
        
        if (subcmd.equalsIgnoreCase(F("CheckCurrents"))) {
          Plugin_180_checkCurrentGuest(address, door, tag);
          success = true;
        }
      }
      
      // GetCurrents command for updating data
      if (cmd.equalsIgnoreCase(F("GetCurrents"))) {
        String subcmd = parseString(string, 2);
        String address = parseStringKeepCase(string, 3);
        String door = parseStringKeepCase(string, 4);
        
        if (subcmd.equalsIgnoreCase(F("CheckCurrents"))) {
          String path = address + "/" + door;
          Plugin_180_getCurrents(path);
          success = true;
        }
      }
      
      break;
    }

    case PLUGIN_FIFTY_PER_SECOND: {
      // Check for auto-update
      if (Plugin_180_Data.autoUpdate && 
          (millis() - Plugin_180_Data.lastUpdate) >= Plugin_180_Data.updateInterval) {
        
        bool updated = Plugin_180_updateData();
        UserVar.setFloat(event->TaskIndex, 0, updated ? 1.0 : 0.0);
        Plugin_180_Data.lastUpdate = millis();
        
        if (updated) {
          sendData(event);
        }
      }
      break;
    }

    case PLUGIN_READ: {
      // Read current status
      String fileContent = Plugin_180_readFile(Plugin_180_Data.dataFilename);
      UserVar.setFloat(event->TaskIndex, 0, fileContent.length() > 0 ? 1.0 : 0.0);
      success = true;
      break;
    }
  }

  return success;
}

#endif // USES_P099
