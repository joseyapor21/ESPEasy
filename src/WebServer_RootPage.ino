#ifdef WEBSERVER_ROOT

#include "src/Commands/InternalCommands.h"
#include "src/Globals/Nodes.h"

// ********************************************************************************
// Web Interface root page
// ********************************************************************************
void handle_root() {
  checkRAM(F("handle_root"));

  // if Wifi setup, launch setup wizard
  if (wifiSetup)
  {
    web_server.send(200, F("text/html"), F("<meta HTTP-EQUIV='REFRESH' content='0; url=/setup'>"));
    return;
  }

  if (!isLoggedIn()) { return; }
  navMenuIndex = 0;

  // if index.htm exists on FS serve that one (first check if gziped version exists)
  if (loadFromFS(true, F("/index.htm.gz"))) { return; }

  if (loadFromFS(false, F("/index.htm.gz"))) { return; }

  if (loadFromFS(true, F("/index.htm"))) { return; }

  if (loadFromFS(false, F("/index.htm"))) { return; }

  TXBuffer.startStream();
  String  sCommand  = web_server.arg(F("cmd"));
  boolean rebootCmd = strcasecmp_P(sCommand.c_str(), PSTR("reboot")) == 0;
  sendHeadandTail_stdtemplate(_HEAD, rebootCmd);

  int freeMem = ESP.getFreeHeap();


  if ((strcasecmp_P(sCommand.c_str(),
                    PSTR("wifidisconnect")) != 0) && (rebootCmd == false) && (strcasecmp_P(sCommand.c_str(), PSTR("reset")) != 0))
  {
    printToWeb     = true;
    printWebString = "";

    if (sCommand.length() > 0) {
      ExecuteCommand_internal(EventValueSource::Enum::VALUE_SOURCE_HTTP, sCommand.c_str());
    }

    // IPAddress ip = NetworkLocalIP();
    // IPAddress gw = NetwrokGatewayIP();

    addHtml(printWebString);
    addHtml(F("<form>"));
    html_table_class_normal();
    addFormHeader(F("System Info"));

    addRowLabelValue(LabelType::UNIT_NR);
    addRowLabelValue(LabelType::GIT_BUILD);
    addRowLabel(getLabel(LabelType::LOCAL_TIME));

    if (node_time.systemTimePresent())
    {
      addHtml(getValue(LabelType::LOCAL_TIME));
    }
    else {
      addHtml(F("<font color='red'>No system time source</font>"));
    }
    addRowLabelValue(LabelType::TIME_SOURCE);

    addRowLabel(getLabel(LabelType::UPTIME));
    {
      addHtml(getExtendedValue(LabelType::UPTIME));
    }
    addRowLabel(getLabel(LabelType::LOAD_PCT));

    if (wdcounter > 0)
    {
      String html;
      html.reserve(32);
      html += getCPUload();
      html += F("% (LC=");
      html += getLoopCountPerSec();
      html += ')';
      addHtml(html);
    }
    {
      addRowLabel(getLabel(LabelType::FREE_MEM));
      String html;
      html.reserve(64);
      html += freeMem;
      html += " (";
      html += lowestRAM;
      html += F(" - ");
      html += lowestRAMfunction;
      html += ')';
      addHtml(html);
    }
    {
      addRowLabel(getLabel(LabelType::FREE_STACK));
      String html;
      html.reserve(64);
      html += String(getCurrentFreeStack());
      html += " (";
      html += String(lowestFreeStack);
      html += F(" - ");
      html += String(lowestFreeStackfunction);
      html += ')';
      addHtml(html);
    }

    addRowLabelValue(LabelType::IP_ADDRESS);
    addRowLabel(getLabel(LabelType::WIFI_RSSI));

    if (NetworkConnected())
    {
      String html;
      html.reserve(32);
      html += String(WiFi.RSSI());
      html += F(" dB (");
      html += WiFi.SSID();
      html += ')';
      addHtml(html);
    }

#ifdef HAS_ETHERNET
    addRowLabelValue(LabelType::ETH_WIFI_MODE);
    if(eth_wifi_mode == ETHERNET) {
      addRowLabelValue(LabelType::ETH_SPEED_STATE);
      addRowLabelValue(LabelType::ETH_IP_ADDRESS);
    }
#endif

    #ifdef FEATURE_MDNS
    {
      addRowLabel(getLabel(LabelType::M_DNS));
      String html;
      html.reserve(64);
      html += F("<a href='http://");
      html += getValue(LabelType::M_DNS);
      html += F("'>");
      html += getValue(LabelType::M_DNS);
      html += F("</a>");
      addHtml(html);
    }
    #endif // ifdef FEATURE_MDNS
    html_TR_TD();
    html_TD();
    addButton(F("sysinfo"), F("More info"));

    html_end_table();
    html_BR();
    html_BR();
    html_table_class_multirow_noborder();
    html_TR();
    html_table_header(F("Node List"));
    html_table_header("Name");
    html_table_header(getLabel(LabelType::BUILD_DESC));
    html_table_header("Type");
    html_table_header("IP", 160); // Should fit "255.255.255.255"
    html_table_header("Load");
    html_table_header("Age (s)");
    #ifdef USES_ESPEASY_NOW
    html_table_header("Dist");
    #endif

    for (auto it = Nodes.begin(); it != Nodes.end(); ++it)
    {
      if (it->second.valid())
      {
        bool isThisUnit = it->first == Settings.Unit;

        if (isThisUnit) {
          html_TR_TD_highlight();
        }
        else {
          html_TR_TD();
        }

        addHtml(F("Unit "));
        addHtml(String(it->first));
        html_TD();

        if (isThisUnit) {
          addHtml(Settings.Name);
        }
        else {
          addHtml(it->second.getNodeName());
        }
        html_TD();

        if (it->second.build) {
          addHtml(String(it->second.build));
        }
        html_TD();
        addHtml(it->second.getNodeTypeDisplayString());
        html_TD();
        if (it->second.ip[0] != 0)
        {
          html_add_wide_button_prefix();
          String html;
          html.reserve(64);

          html += F("http://");
          html += it->second.IP().toString();
          uint16_t port = it->second.webgui_portnumber;
          if (port !=0 && port != 80) {
            html += ':';
            html += String(port);
          }
          html += "'>";
          html += it->second.IP().toString();
          html += "</a>";
          addHtml(html);
        } else if (it->second.ESPEasyNowPeer) {
          addHtml(F("ESPEasy-Now "));
          addHtml(it->second.ESPEasy_Now_MAC().toString());
          addHtml(F(" (ch: "));
          addHtml(String(it->second.channel));
          addHtml(F(")"));
        }
        html_TD();
        const float load = it->second.getLoad();
        if (load > 0.1) {
          addHtml(String(it->second.getLoad()));
        }
        html_TD();
        addHtml(String(it->second.getAge()/1000)); // time in seconds
        #ifdef USES_ESPEASY_NOW
        html_TD();
        if (it->second.distance != 255) {
          addHtml(String(it->second.distance)); 
        }
        #endif
      }
    }

    html_end_table();
    html_end_form();

    printWebString = "";
    printToWeb     = false;
    sendHeadandTail_stdtemplate(_TAIL);
    TXBuffer.endStream();
  }
  else
  {
    // TODO: move this to handle_tools, from where it is actually called?

    // have to disconnect or reboot from within the main loop
    // because the webconnection is still active at this point
    // disconnect here could result into a crash/reboot...
    if (strcasecmp_P(sCommand.c_str(), PSTR("wifidisconnect")) == 0)
    {
      addLog(LOG_LEVEL_INFO, F("WIFI : Disconnecting..."));
      cmd_within_mainloop = CMD_WIFI_DISCONNECT;
    }

    if (strcasecmp_P(sCommand.c_str(), PSTR("reboot")) == 0)
    {
      addLog(LOG_LEVEL_INFO, F("     : Rebooting..."));
      cmd_within_mainloop = CMD_REBOOT;
    }

    if (strcasecmp_P(sCommand.c_str(), PSTR("reset")) == 0)
    {
      addLog(LOG_LEVEL_INFO, F("     : factory reset..."));
      cmd_within_mainloop = CMD_REBOOT;
      addHtml(F(
                "OK. Please wait > 1 min and connect to Acces point.<BR><BR>PW=configesp<BR>URL=<a href='http://192.168.4.1'>192.168.4.1</a>"));
      TXBuffer.endStream();
      ExecuteCommand_internal(EventValueSource::Enum::VALUE_SOURCE_HTTP, sCommand.c_str());
    }

    addHtml(F("OK"));
    TXBuffer.endStream();
  }
}

#endif // ifdef WEBSERVER_ROOT
