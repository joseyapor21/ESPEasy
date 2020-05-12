#include "src/Commands/InternalCommands.h"
#include "src/Globals/Nodes.h"
#include "src/Globals/Device.h"
#include "src/Globals/Plugins.h"

// ********************************************************************************
// Web Interface custom page handler
// ********************************************************************************
boolean handle_custom(String path) {
  // path is a deepcopy, since it will be changed.
  checkRAM(F("handle_custom"));

  if (!clientIPallowed()) { return false; }

#if !defined(ESP32)
  path = path.substring(1);
#endif // if !defined(ESP32)

  // create a dynamic custom page, parsing task values into [<taskname>#<taskvalue>] placeholders and parsing %xx% system variables
  fs::File   dataFile      = tryOpenFile(path.c_str(), "r");
  const bool dashboardPage = path.startsWith(F("dashboard"));

  if (!dataFile && !dashboardPage) {
    return false;    // unknown file that does not exist...
  }

  if (dashboardPage) // for the dashboard page, create a default unit dropdown selector
  {
    // handle page redirects to other unit's as requested by the unit dropdown selector
    byte unit    = getFormItemInt(F("unit"));
    byte btnunit = getFormItemInt(F("btnunit"));

    if (!unit) { unit = btnunit; // unit element prevails, if not used then set to btnunit
    }

    if (unit && (unit != Settings.Unit))
    {
      auto it = Nodes.find(unit);

      if (it != Nodes.end()) {
        TXBuffer.startStream();
        sendHeadandTail(F("TmplDsh"), _HEAD);
        addHtml(F("<meta http-equiv=\"refresh\" content=\"0; URL=http://"));
        addHtml(it->second.IP().toString());
        addHtml(F("/dashboard.esp\">"));
        sendHeadandTail(F("TmplDsh"), _TAIL);
        TXBuffer.endStream();
        return true;
      }
    }

    TXBuffer.startStream();
    sendHeadandTail(F("TmplDsh"), _HEAD);
    html_add_autosubmit_form();
    html_add_form();

    // create unit selector dropdown
    addSelector_Head_reloadOnChange(F("unit"));
    byte choice = Settings.Unit;

    for (auto it = Nodes.begin(); it != Nodes.end(); ++it)
    {
      if ((it->second.ip[0] != 0) || (it->first == Settings.Unit))
      {
        String name = String(it->first) + F(" - ");

        if (it->first != Settings.Unit) {
          name += it->second.getNodeName();
        }
        else {
          name += Settings.Name;
        }
        addSelector_Item(name, it->first, choice == it->first, false, "");
      }
    }
    addSelector_Foot();

    // create <> navigation buttons
    byte prev = Settings.Unit;
    byte next = Settings.Unit;

    for (byte x = Settings.Unit - 1; x > 0; x--) {
      auto it = Nodes.find(x);

      if (it != Nodes.end()) {
        if (it->second.ip[0] != 0) { prev = x; break; }
      }
    }

    for (byte x = Settings.Unit + 1; x < UNIT_NUMBER_MAX; x++) {
      auto it = Nodes.find(x);

      if (it != Nodes.end()) {
        if (it->second.ip[0] != 0) { next = x; break; }
      }
    }

    html_add_button_prefix();
    addHtml(path);
    addHtml(F("?btnunit="));
    addHtml(String(prev));
    addHtml(F("'>&lt;</a>"));
    html_add_button_prefix();
    addHtml(path);
    addHtml(F("?btnunit="));
    addHtml(String(next));
    addHtml(F("'>&gt;</a>"));
  }

  // handle commands from a custom page
  String webrequest = web_server.arg(F("cmd"));

  if (webrequest.length() > 0) {
    ExecuteCommand_all_config_eventOnly(EventValueSource::Enum::VALUE_SOURCE_HTTP, webrequest.c_str());

    // handle some update processes first, before returning page update...
    String dummy;
    PluginCall(PLUGIN_TEN_PER_SECOND, 0, dummy);
  }


  if (dataFile)
  {
    String page = "";
    page.reserve(dataFile.size());

    while (dataFile.available()) {
      page += ((char)dataFile.read());
    }

    addHtml(parseTemplate(page));
    dataFile.close();
  }
  else // if the requestef file does not exist, create a default action in case the page is named "dashboard*"
  {
    if (dashboardPage)
    {
      // if the custom page does not exist, create a basic task value overview page in case of dashboard request...
      addHtml(F(
                "<meta name='viewport' content='width=width=device-width, initial-scale=1'><STYLE>* {font-family:sans-serif; font-size:16pt;}.button {margin:4px; padding:4px 16px; background-color:#07D; color:#FFF; text-decoration:none; border-radius:4px}</STYLE>"));
      html_table_class_normal();

      for (taskIndex_t x = 0; x < TASKS_MAX; x++)
      {
        if (validPluginID_fullcheck(Settings.TaskDeviceNumber[x]))
        {
          const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(x);

          if (validDeviceIndex(DeviceIndex)) {
            LoadTaskSettings(x);
            html_TR_TD();
            addHtml(ExtraTaskSettings.TaskDeviceName);

            for (byte varNr = 0; varNr < VARS_PER_TASK; varNr++)
            {
              if ((varNr < Device[DeviceIndex].ValueCount) &&
                  (ExtraTaskSettings.TaskDeviceValueNames[varNr][0] != 0))
              {
                if (varNr > 0) {
                  html_TR_TD();
                }
                html_TD();
                addHtml(ExtraTaskSettings.TaskDeviceValueNames[varNr]);
                html_TD();
                addHtml(String(UserVar[x * VARS_PER_TASK + varNr], ExtraTaskSettings.TaskDeviceValueDecimals[varNr]));
              }
            }
          }
        }
      }
    }
  }
  sendHeadandTail(F("TmplDsh"), _TAIL);
  TXBuffer.endStream();
  return true;
}
