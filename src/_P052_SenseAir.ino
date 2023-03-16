#include "_Plugin_Helper.h"
#ifdef USES_P052

# include "src/PluginStructs/P052_data_struct.h"

// #######################################################################################################
// ############################# Plugin 052: Senseair CO2 Sensors ########################################
// #######################################################################################################

/*
   Plugin originally written by: Daniel Tedenljung
   info__AT__tedenljungconsulting.com
   Rewritten by: Mikael Trieb mikael__AT__triebconsulting.se

   This plugin reads available values of Senseair Co2 Sensors.
   Datasheet can be found here:
   S8: http://www.senseair.com/products/oem-modules/senseair-s8/
   K30: http://www.senseair.com/products/oem-modules/k30/
   K70/tSENSE: http://www.senseair.com/products/wall-mount/tsense/

   Circuit wiring
    GPIO Setting 1 -> RX
    GPIO Setting 2 -> TX
    Use 1kOhm in serie on datapins!
 */

# define PLUGIN_052
# define PLUGIN_ID_052 52
# define PLUGIN_NAME_052 "Gases - CO2 Senseair"


boolean Plugin_052(uint8_t function, struct EventStruct *event, String& string) {
  boolean success = false;

  switch (function) {
    case PLUGIN_DEVICE_ADD: {
      Device[++deviceCount].Number           = PLUGIN_ID_052;
      Device[deviceCount].Type               = DEVICE_TYPE_SERIAL;
      Device[deviceCount].VType              = Sensor_VType::SENSOR_TYPE_SINGLE;
      Device[deviceCount].Ports              = 0;
      Device[deviceCount].PullUpOption       = false;
      Device[deviceCount].InverseLogicOption = false;
      Device[deviceCount].FormulaOption      = true;
      Device[deviceCount].ValueCount         = 1;
      Device[deviceCount].SendDataOption     = true;
      Device[deviceCount].TimerOption        = true;
      Device[deviceCount].GlobalSyncOption   = true;
      Device[deviceCount].OutputDataType     = Output_Data_type_t::Simple;
      Device[deviceCount].ExitTaskBeforeSave = false;
      Device[deviceCount].PluginStats        = true;
      break;
    }

    case PLUGIN_GET_DEVICENAME: {
      string = F(PLUGIN_NAME_052);
      break;
    }

    case PLUGIN_GET_DEVICEVALUENAMES: {
      for (uint8_t i = 0; i < VARS_PER_TASK; ++i) {
        if (i < P052_NR_OUTPUT_VALUES) {
          const uint8_t pconfigIndex = i + P052_QUERY1_CONFIG_POS;
          uint8_t choice             = PCONFIG(pconfigIndex);
          safe_strncpy(
            ExtraTaskSettings.TaskDeviceValueNames[i],
            P052_data_struct::Plugin_052_valuename(choice, false),
            sizeof(ExtraTaskSettings.TaskDeviceValueNames[i]));
        } else {
          ZERO_FILL(ExtraTaskSettings.TaskDeviceValueNames[i]);
        }
      }
      break;
    }

    case PLUGIN_GET_DEVICEVALUECOUNT:
    {
      event->Par1 = P052_NR_OUTPUT_VALUES;
      success     = true;
      break;
    }

    case PLUGIN_GET_DEVICEVTYPE:
    {
      event->sensorType = static_cast<Sensor_VType>(PCONFIG(P052_SENSOR_TYPE_INDEX));
      event->idx        = P052_SENSOR_TYPE_INDEX;
      success           = true;
      break;
    }

    case PLUGIN_GET_DEVICEGPIONAMES: {
      serialHelper_getGpioNames(event);
      break;
    }

    case PLUGIN_WEBFORM_SHOW_CONFIG:
    {
      string += serialHelper_getSerialTypeLabel(event);
      success = true;
      break;
    }

    case PLUGIN_SET_DEFAULTS:
    {
      PCONFIG(P052_SENSOR_TYPE_INDEX) = static_cast<int16_t>(Sensor_VType::SENSOR_TYPE_SINGLE);
      PCONFIG(0)                      = 1; // "CO2"

      for (uint8_t i = 1; i < VARS_PER_TASK; ++i) {
        PCONFIG(i) = 0;                    // "Empty"
      }

      success = true;
      break;
    }

    case PLUGIN_WEBFORM_LOAD_OUTPUT_SELECTOR:
    {
      const __FlashStringHelper *options[P052_NR_OUTPUT_OPTIONS];

      for (uint8_t i = 0; i < P052_NR_OUTPUT_OPTIONS; ++i) {
        options[i] = P052_data_struct::Plugin_052_valuename(i, true);
      }

      for (uint8_t i = 0; i < P052_NR_OUTPUT_VALUES; ++i) {
        const uint8_t pconfigIndex = i + P052_QUERY1_CONFIG_POS;
        sensorTypeHelper_loadOutputSelector(event, pconfigIndex, i, P052_NR_OUTPUT_OPTIONS, options);
      }
      break;
    }

    case PLUGIN_WEBFORM_LOAD: {
      P052_data_struct *P052_data =
        static_cast<P052_data_struct *>(getPluginTaskData(event->TaskIndex));

      if ((nullptr != P052_data) && P052_data->isInitialized()) {
        addFormSubHeader(F("Device Information"));
        {
          int value = 0;

          if (P052_data->modbus.detected_device_description.length() > 0) {
            addRowLabel(F("Detected Device"));
            addHtml(P052_data->modbus.detected_device_description);
          }

          if (P052_data->readInputRegister(P052_IR29_FW_REV, value)) {
            addRowLabel(F("FW rev."));
            addHtmlInt(value);
          }

          addRowLabel(F("Sensor ID"));
          addHtml(formatToHex_decimal(P052_data->getSensorID()));


          addRowLabel(F("Checksum (pass/fail/nodata)"));
          {
            uint32_t reads_pass, reads_crc_failed, reads_nodata;
            P052_data->modbus.getStatistics(reads_pass, reads_crc_failed, reads_nodata);
            String chksumStats;
            chksumStats  = reads_pass;
            chksumStats += '/';
            chksumStats += reads_crc_failed;
            chksumStats += '/';
            chksumStats += reads_nodata;
            addHtml(chksumStats);
          }

          bool hasFactorySettings = false;
          bool enabledABC         = false;

          if (P052_data->readHoldingRegister(P052_HR19_METER_CONTROL, value)) {
            hasFactorySettings = value == 255;
            enabledABC         = bitRead(value, 1) == 0;
            addRowLabel(F("Using Factory Settings"));
            addEnabled(hasFactorySettings);

            if (!hasFactorySettings) {
              // addRowLabel(F("HR19"));
              // addHtmlInt(value);
              addRowLabel(F("nRDY"));
              addEnabled(bitRead(value, 0) == 0);
              addRowLabel(F("ABC"));
              addEnabled(enabledABC);
              addRowLabel(F("Static IIR filter"));
              addEnabled(bitRead(value, 2) == 0);
              addRowLabel(F("Dynamic IIR filter"));
              addEnabled(bitRead(value, 3) == 0);
              addRowLabel(F("Pressure Compensation"));
              addEnabled(bitRead(value, 4) == 0);
            }
          }


          if (P052_data->readInputRegister(P052_IR7_MEASUREMENT_COUNT, value)) {
            addRowLabel(F("Measurement Count"));
            addHtmlInt(value);
          }

          if (P052_data->readInputRegister(P052_IR8_MEASUREMENT_CYCLE_TIME, value)) {
            addRowLabel(F("Measurement Cycle time"));
            addHtmlInt(value * 2);
            addUnit('s');
          }

          if (P052_data->readInputRegister(P052_IR9_MEASURED_UNFILTERED_CO2, value)) {
            addRowLabel(F("Unfiltered CO2"));
            addHtmlInt(value);
          }

          if (P052_data->readHoldingRegister(P052_HR11_MEASUREMENT_MODE, value)) {
            addRowLabel(F("Measurement Mode"));
            addHtml(value == 0 ? F("Continuous") : F("Single Measurement"));
          }

          if (enabledABC) {
            if (P052_data->readHoldingRegister(P052_HR14_ABC_PERIOD, value)) {
              addRowLabel(F("ABC Period"));
              addHtmlInt(value);
              addUnit('h');
            }

            if (P052_data->readHoldingRegister(P052_HR5_ABC_TIME, value)) {
              addRowLabel(F("Time Since ABC"));
              addHtmlInt(value);
              addUnit('h');
            }
          }
        }

        {
          int value = 0;

          /*
              if (P052_data->readHoldingRegister(P052_HR11_MEASUREMENT_MODE, value)) {
                // Disable selector for now, since single measurement not yet supported.

                const __FlashStringHelper *options[2] = { F("Continuous"), F("Single Measurement") };
                addFormSelector(F("Measurement Mode"), F("mode"), 2, options, nullptr, value);
              }
           */

          if (P052_data->readHoldingRegister(P052_HR12_MEASUREMENT_PERIOD, value)) {
            addFormNumericBox(F("Measurement Period"), F("period"), value, 2, 65534);
            addUnit('s');
          }

          if (P052_data->readHoldingRegister(P052_HR13_NR_OF_SAMPLES, value)) {
            addFormNumericBox(F("Samples per measurement"), F("samp_meas"), value, 1, 1024);
          }
        }
        {
          // P052_ABC_PERIOD
        }
      }

      /*
         // ABC functionality disabled for now, due to a bug in the firmware.
         // See https://github.com/letscontrolit/ESPEasy/issues/759
         uint8_t choiceABCperiod = PCONFIG(4);
         const __FlashStringHelper * optionsABCperiod[9] = { F("disable"), F("1 h"), F("12 h"), F("1
         day"), F("2 days"), F("4 days"), F("7 days"), F("14 days"), F("30 days") };
         addFormSelector(F("ABC period"), F("ABC_period"), 9, optionsABCperiod,
         nullptr, choiceABCperiod);
       */


      success = true;
      break;
    }

    case PLUGIN_WEBFORM_SAVE: {
      // Save output selector parameters.
      for (uint8_t i = 0; i < P052_NR_OUTPUT_VALUES; ++i) {
        const uint8_t pconfigIndex = i + P052_QUERY1_CONFIG_POS;
        const uint8_t choice       = PCONFIG(pconfigIndex);
        sensorTypeHelper_saveOutputSelector(event, pconfigIndex, i, P052_data_struct::Plugin_052_valuename(choice, false));
      }

      P052_data_struct *P052_data =
        static_cast<P052_data_struct *>(getPluginTaskData(event->TaskIndex));

      if ((nullptr != P052_data) && P052_data->isInitialized()) {
        bool changed  = false;
        uint16_t mode = getFormItemInt(F("mode"), 65535);

        if (((mode == 0) || (mode == 1))) {
          uint8_t errorcode;
          int     readVal = P052_data->modbus.readHoldingRegister(0x0A, errorcode);

          if ((errorcode == 0) && (readVal != mode)) {
            P052_data->modbus.writeMultipleRegisters(0x0A, mode);
            delay(0);
            changed = true;
          }
        }
        uint16_t period = getFormItemInt(F("period"), 0);

        if (period > 1) {
          uint8_t errorcode;
          int     readVal = P052_data->modbus.readHoldingRegister(0x0B, errorcode);

          if ((errorcode == 0) && (readVal != period)) {
            P052_data->modbus.writeMultipleRegisters(0x0B, period);
            delay(0);
            changed = true;
          }
        }
        uint16_t samp_meas = getFormItemInt(F("samp_meas"), 0);

        if ((samp_meas > 0) && (samp_meas <= 1024)) {
          uint8_t errorcode;
          int     readVal = P052_data->modbus.readHoldingRegister(0x0C, errorcode);

          if ((errorcode == 0) && (readVal != samp_meas)) {
            P052_data->modbus.writeMultipleRegisters(0x0C, samp_meas);
            delay(0);
            changed = true;
          }
        }

        if (changed) {
          // Restart sensor.
          P052_data->modbus.writeMultipleRegisters(0x11, 0xFF);

          // FIXME TD-er: Must leave the sensor to boot for a while.
          delay(35);
        }
      }


      /*
         // ABC functionality disabled for now, due to a bug in the firmware.
         // See https://github.com/letscontrolit/ESPEasy/issues/759
         PCONFIG(4) = getFormItemInt(F("ABC_period"));
       */

      success = true;
      break;
    }

    case PLUGIN_INIT: {
      const int16_t serial_rx      = CONFIG_PIN1;
      const int16_t serial_tx      = CONFIG_PIN2;
      const ESPEasySerialPort port = static_cast<ESPEasySerialPort>(CONFIG_PORT);
      initPluginTaskData(event->TaskIndex, new (std::nothrow) P052_data_struct());
      P052_data_struct *P052_data =
        static_cast<P052_data_struct *>(getPluginTaskData(event->TaskIndex));

      if (nullptr == P052_data) {
        return success;
      }


      if (P052_data->init(port, serial_rx, serial_tx)) {
        /*
           // ABC functionality disabled for now, due to a bug in the firmware.
           // See https://github.com/letscontrolit/ESPEasy/issues/759
           const int periodInHours[9] = {0, 1, 12, (24*1), (24*2), (24*4), (24*7),
           (24*14), (24*30) };
           uint8_t choiceABCperiod = PCONFIG(1);

           Plugin_052_setABCperiod(periodInHours[choiceABCperiod]);
         */
        P052_data->modbus.setModbusTimeout(P052_MODBUS_TIMEOUT);

        //      P052_data->modbus.writeMultipleRegisters(0x09, 1); // Start Single Measurement
        //      P052_data->modbus.writeMultipleRegisters(0x0B, 16); // Measurement Period
        //      P052_data->modbus.writeMultipleRegisters(0x0C, 8); // Number of samples

        success = true;
      } else {
        clearPluginTaskData(event->TaskIndex);
      }
      break;
    }

    case PLUGIN_EXIT: {
      success = true;
      break;
    }

    case PLUGIN_READ: {
      P052_data_struct *P052_data =
        static_cast<P052_data_struct *>(getPluginTaskData(event->TaskIndex));

      if ((nullptr != P052_data) && P052_data->isInitialized()) {
        event->sensorType = static_cast<Sensor_VType>(PCONFIG(P052_SENSOR_TYPE_INDEX));
        String log = F("Senseair: ");
        String logPrefix;

        for (int varnr = 0; varnr < P052_NR_OUTPUT_VALUES; ++varnr) {
          uint8_t errorcode = 0;
          float   value     = 0;

          switch (PCONFIG(varnr)) {
            case 1: {
              value     = P052_data->modbus.readInputRegister(P052_IR4_MEASURED_FILTERED_CO2, errorcode);
              logPrefix = F("co2 = ");
              break;
            }
            case 2: {
              int temperatureX100 = P052_data->modbus.readInputRegister(P052_IR5_TEMPERATURE, errorcode);

              if (errorcode != 0) {
                // SenseAir S8, not for other modules.
                temperatureX100 = P052_data->modbus.read_RAM_EEPROM(
                  P052_CMD_READ_RAM, P052_RAM_ADDR_DET_TEMPERATURE, 2, errorcode);
              }
              value     = static_cast<float>(temperatureX100) / 100.0f;
              logPrefix = F("temperature = ");
              break;
            }
            case 3: {
              int rhX100 = P052_data->modbus.readInputRegister(P052_IR6_SPACE_HUMIDITY, errorcode);
              value     = static_cast<float>(rhX100) / 100.0f;
              logPrefix = F("humidity = ");
              break;
            }
            case 4: {
              int status = P052_data->modbus.readInputRegister(P052_IR29_FW_REV, errorcode);

              if (errorcode == 0) {
                int relayStatus = (status >> 8) & 0x1;
                UserVar[event->BaseVarIndex + varnr] = relayStatus;
                log                                 += F("relay status = ");
                log                                 += relayStatus;
              }
              break;
            }
            case 5: {
              int temperatureAdjustment = P052_data->modbus.readInputRegister(P052_IR11_MEASURED_CONCENTRATION_UNFILTERED, errorcode);
              value     = static_cast<float>(temperatureAdjustment);
              logPrefix = F("temperature adjustment = ");
              break;
            }

            case 7: {
              const int errorWord = P052_data->modbus.readInputRegister(P052_IR_ERRORSTATUS, errorcode);

              if (errorcode == 0) {
                value = errorWord;

                for (size_t i = 0; i < 9; i++) {
                  if (bitRead(errorWord, i)) {
                    log += F("error code = ");
                    log += i;
                    break;
                  }
                }
              } else {
                value = -1;
                log  += F("error code = ");
                log  += -1;
              }
              break;
            }
            case 0:
            default: {
              UserVar[event->BaseVarIndex + varnr] = 0;
              break;
            }
          }

          if (P052_data->modbus.getLastError() == 0) {
            success                              = true;
            UserVar[event->BaseVarIndex + varnr] = value;
            log                                 += logPrefix;
            log                                 += value;
          }
        }
        addLogMove(LOG_LEVEL_INFO, log);
        break;
      }
      break;
    }

    case PLUGIN_WRITE:
    {
      P052_data_struct *P052_data =
        static_cast<P052_data_struct *>(getPluginTaskData(event->TaskIndex));

      if ((nullptr != P052_data) && P052_data->isInitialized()) {
        const String command = parseString(string, 1);

        if (equals(command, F("senseair"))) {
          const String subcommand = parseString(string, 2);

          if (equals(subcommand, F("writehr"))) {
            // FIXME TD-er: Must test, never tested on real hardware yet....

            uint32_t addr = 0;
            uint32_t cmnd = 0;

            if (validUIntFromString(parseString(string, 3), addr) &&
                validUIntFromString(parseString(string, 4), cmnd)) {
              uint8_t errorcode = 0;
              P052_data->modbus.writeSingleRegister(addr, cmnd, errorcode);
              success = 0 == errorcode;
            }
          } else if (equals(subcommand, F("readhr"))) {
            uint32_t addr = 0;

            if (validUIntFromString(parseString(string, 3), addr)) {
              uint8_t   errorcode = 0;
              const int value     = P052_data->modbus.readHoldingRegister(addr, errorcode);

              if (0 == errorcode) {
                success = true;

                if (Settings.UseRules) {
                  String eventvalues;
                  eventvalues += (addr + 1); // HR1 = addr 0x00
                  eventvalues += ',';
                  eventvalues += value;
                  eventQueue.add(event->TaskIndex, F("readhr"), eventvalues);
                }
              }
            }
          } else if (equals(subcommand, F("enableabc"))) {
            uint32_t hours = 0;

            if (validUIntFromString(parseString(string, 3), hours)) {
              // FIXME TD-er: Implement


              // Read HR19

              // Clear bit 1 in register and write back HR19

              // Read HR14 and verify desired ABC period

              // If HR14 (ABC period) is not the desired period,
              // write desired ABC period to HR14
            }
          } else if (equals(subcommand, F("disableabc"))) {
            // FIXME TD-er: Implement

            // Read HR19

            // Set bit 1 in register and write back HR19
          } else if (equals(subcommand, F("setabcperiod"))) {
            // FIXME TD-er: Must test, never tested on real hardware yet....

            int period = 0;

            if (validIntFromString(parseString(string, 3), period)) {
              if (period >= 0) {
                P052_data->setABCperiod(period);
                addLog(LOG_LEVEL_INFO, String(F("Senseair command: ABCperiod=")) +
                       period);
                success = true;
              }
            }
          } else if (equals(subcommand, F("setrelay"))) {
            int state = 0;

            if (validIntFromString(parseString(string, 3), state)) {
              short relaystatus = 0; // 0x3FFF represents 100% output.
              success = true;

              //  Refer to sensor model’s specification for voltage at 100% output.
              if (state == 0) { relaystatus = 0; }
              else if (state == 1) { relaystatus = 0x3FFF; }
              else if (state == -1) { relaystatus = 0x7FFF; }
              else { success = false; }

              if (success) {
                P052_data->modbus.writeSingleRegister(0x18, relaystatus);
                addLog(LOG_LEVEL_INFO, concat(F("Senseair command: relay="), state));
              }
            }
          }
        }
      }
      break;
    }
  }
  return success;
}

#endif // USES_P052
