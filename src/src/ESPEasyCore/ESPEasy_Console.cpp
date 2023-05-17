#include "../ESPEasyCore/ESPEasy_Console.h"

#include "../Commands/InternalCommands.h"

#include "../DataTypes/ESPEasy_plugin_functions.h"

#include "../Globals/Cache.h"
#include "../Globals/Logging.h"
#include "../Globals/Plugins.h"
#include "../Globals/Settings.h"

#include "../Helpers/Memory.h"

/*
 #if FEATURE_DEFINE_SERIAL_CONSOLE_PORT
 # include "../Helpers/_Plugin_Helper_serial.h"
 #endif // if FEATURE_DEFINE_SERIAL_CONSOLE_PORT
 */



#ifdef ESP32

  /*
   #if CONFIG_IDF_TARGET_ESP32C3 ||  // support USB via HWCDC using JTAG interface
       CONFIG_IDF_TARGET_ESP32S2 ||  // support USB via USBCDC
       CONFIG_IDF_TARGET_ESP32S3     // support USB via HWCDC using JTAG interface or USBCDC
   */
# if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3

  // #if CONFIG_TINYUSB_CDC_ENABLED              // This define is not recognized here so use USE_USB_CDC_CONSOLE
#  ifdef USE_USB_CDC_CONSOLE
#   if ARDUINO_USB_MODE

  // ESP32C3/S3 embedded USB using JTAG interface
HWCDC _hwcdc_serial;
#   else // No ARDUINO_USB_MODE
USBCDC _usbcdc_serial;
#   endif // ARDUINO_USB_MODE
#  endif  // ifdef USE_USB_CDC_CONSOLE
# endif   // if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3
#endif    // ifdef ESP32



EspEasy_Console_t::EspEasy_Console_t() {
#if FEATURE_DEFINE_SERIAL_CONSOLE_PORT
  _serial = new ESPeasySerial(
    static_cast<ESPEasySerialPort>(_console_serial_port),
    _console_serial_rxpin,
    _console_serial_txpin,
    false,
    64);

#endif // if FEATURE_DEFINE_SERIAL_CONSOLE_PORT


#ifdef ESP32

  /*
   #if CONFIG_IDF_TARGET_ESP32C3 ||  // support USB via HWCDC using JTAG interface
       CONFIG_IDF_TARGET_ESP32S2 ||  // support USB via USBCDC
       CONFIG_IDF_TARGET_ESP32S3     // support USB via HWCDC using JTAG interface or USBCDC
   */

  /*
   # if CONFIG_IDF_TARGET_ESP32C3 || CONFIG_IDF_TARGET_ESP32S2 || CONFIG_IDF_TARGET_ESP32S3

     // #if CONFIG_TINYUSB_CDC_ENABLED              // This define is not recognized here so use USE_USB_CDC_CONSOLE
   #  ifdef USE_USB_CDC_CONSOLE
   #   if ARDUINO_USB_MODE

     // ESP32C3/S3 embedded USB using JTAG interface
   #    warning **** ESPEasy_Console uses HWCDC ****
   #   else // No ARDUINO_USB_MODE
     // ESP32Sx embedded USB interface
   #    warning **** ESPEasy_Console uses USBCDC ****
   #   endif  // ARDUINO_USB_MODE

   #  else // No USE_USB_CDC_CONSOLE
     // Fallback serial interface for ESP32C3, S2 and S3 if no USB_SERIAL defined
   #   warning **** ESPEasy_Console uses Serial ****
   #  endif  // USE_USB_CDC_CONSOLE

   # else // No ESP32C3, S2 or S3
     // Fallback serial interface for non ESP32C3, S2 and S3
   #  warning **** ESPEasy_Console uses Serial ****
   # endif  // ESP32C3, S2 or S3

   #else // No ESP32
     // Using the standard Serial0 HW serial port.
   # warning **** ESPEasy_Console uses Serial ****
   */
#endif // ifdef ESP32
}

EspEasy_Console_t::~EspEasy_Console_t() {
#if FEATURE_DEFINE_SERIAL_CONSOLE_PORT

  if (_serial != nullptr) {
    delete _serial;
    _serial = nullptr;
  }
#endif // if FEATURE_DEFINE_SERIAL_CONSOLE_PORT
}

void EspEasy_Console_t::begin(uint32_t baudrate)
{
  if (_defaultPortActive) {
#if FEATURE_DEFINE_SERIAL_CONSOLE_PORT
    _serial->begin(baudrate);
    addLog(LOG_LEVEL_INFO, F("ESPEasy console using ESPEasySerial"));
#else // if FEATURE_DEFINE_SERIAL_CONSOLE_PORT
# ifdef ESP8266
    _serial->begin(baudrate);
    addLog(LOG_LEVEL_INFO, F("ESPEasy console using HW Serial"));
# endif // ifdef ESP8266
# ifdef ESP32

    // Allow to flush data from the serial buffers
    // When not opening the USB serial port, the ESP may hang at boot.
    delay(10);
    _serial->end();
    delay(10);
    _serial->begin(baudrate);
    _serial->flush();
# endif // ifdef ESP32
#endif  // if FEATURE_DEFINE_SERIAL_CONSOLE_PORT
  } else {
#if CONSOLE_USES_USBCDC
    _usbcdc_serial.setRxBufferSize(64);
    _usbcdc_serial.begin(baudrate);
    USB.begin();
    addLog(LOG_LEVEL_INFO, F("ESPEasy console using USB CDC"));
#endif // if CONSOLE_USES_USBCDC
#if CONSOLE_USES_HWCDC
    _hwcdc_serial.begin();
    addLog(LOG_LEVEL_INFO, F("ESPEasy console using HWCDC"));

#endif // if CONSOLE_USES_HWCDC
  }
}

void EspEasy_Console_t::init() {
  updateActiveTaskUseSerial0();

  if (!Settings.UseSerial) {
    return;
  }

#if FEATURE_DEFINE_SERIAL_CONSOLE_PORT

  // FIXME TD-er: Must detect whether we should swap software serial on pin 3&1 for HW serial if Serial0 is not being used anymore.
  const ESPEasySerialPort port = static_cast<ESPEasySerialPort>(Settings.console_serial_port);

  if ((port == ESPEasySerialPort::serial0) || (port == ESPEasySerialPort::serial0_swap)) {
    if (activeTaskUseSerial0()) {
      return;
    }
  }
#else // if FEATURE_DEFINE_SERIAL_CONSOLE_PORT

  if (activeTaskUseSerial0()) {
    return;
  }
#endif // if FEATURE_DEFINE_SERIAL_CONSOLE_PORT

  if (log_to_serial_disabled) {
    return;
  }


#if FEATURE_DEFINE_SERIAL_CONSOLE_PORT

  //  if (ESPEASY_SERIAL_CONSOLE_PORT.getSerialPortType() == ESPEasySerialPort::not_set)

  /*
     if (Settings.console_serial_port != console_serial_port ||
        Settings.console_serial_rxpin != console_serial_rxpin ||
        Settings.console_serial_txpin != console_serial_txpin) {

      const uint8_t current_console_serial_port = console_serial_port;
      // Update cached values
      console_serial_port  = Settings.console_serial_port;
      console_serial_rxpin = Settings.console_serial_rxpin;
      console_serial_txpin = Settings.console_serial_txpin;

   #ifdef ESP8266
      bool forceSWSerial = static_cast<ESPEasySerialPort>(Settings.console_serial_port) == ESPEasySerialPort::software;
      if (activeTaskUseSerial0()) {
        if (ESPEasySerialPort::sc16is752 != static_cast<ESPEasySerialPort>(console_serial_port)) {
          forceSWSerial = true;
          console_serial_port = static_cast<uint8_t>(ESPEasySerialPort::software);
        }
      }
   #endif

      if (loglevelActiveFor(LOG_LEVEL_INFO)) {
        String log = F("Serial : Change serial console port from: ");
        log += ESPEasySerialPort_toString(static_cast<ESPEasySerialPort>(current_console_serial_port));
        log += F(" to: ");
        log += ESPEasySerialPort_toString(static_cast<ESPEasySerialPort>(console_serial_port));
   #ifdef ESP8266
        if (forceSWSerial) {
          log += F(" (force SW serial)");
        }
   #endif
        addLogMove(LOG_LEVEL_INFO, log);
      }
      process_serialWriteBuffer();


      ESPEASY_SERIAL_CONSOLE_PORT.resetConfig(
        static_cast<ESPEasySerialPort>(console_serial_port),
        console_serial_rxpin,
        console_serial_txpin,
        false,
        64
   #ifdef ESP8266
        , forceSWSerial
   #endif
        );
     }
   */
#endif // if FEATURE_DEFINE_SERIAL_CONSOLE_PORT

  begin(Settings.BaudRate);
}

void EspEasy_Console_t::loop()
{
  Stream *port = getPort();

  if (port == nullptr) {
    return;
  }

  if (_defaultPortActive && port->available())
  {
    String dummy;

    if (PluginCall(PLUGIN_SERIAL_IN, 0, dummy)) {
      return;
    }
  }
#if FEATURE_DEFINE_SERIAL_CONSOLE_PORT

  // FIXME TD-er: Must add check whether SW serial may be using the same pins as Serial0
  if (!Settings.UseSerial) { return; }
#else // if FEATURE_DEFINE_SERIAL_CONSOLE_PORT

  if (!Settings.UseSerial || activeTaskUseSerial0()) { return; }
#endif // if FEATURE_DEFINE_SERIAL_CONSOLE_PORT

  while (port->available())
  {
    delay(0);
    const uint8_t SerialInByte = port->read();

    if (SerialInByte == 255) // binary data...
    {
      port->flush();
      return;
    }

    if (isprint(SerialInByte))
    {
      if (SerialInByteCounter < CONSOLE_INPUT_BUFFER_SIZE) { // add char to string if it still fits
        InputBuffer_Serial[SerialInByteCounter++] = SerialInByte;
      }
    }

    if ((SerialInByte == '\r') || (SerialInByte == '\n'))
    {
      if (SerialInByteCounter == 0) {              // empty command?
        break;
      }
      InputBuffer_Serial[SerialInByteCounter] = 0; // serial data completed
      addToSerialBuffer('>');
      addToSerialBuffer(String(InputBuffer_Serial));
      ExecuteCommand_all(EventValueSource::Enum::VALUE_SOURCE_SERIAL, InputBuffer_Serial);
      SerialInByteCounter   = 0;
      InputBuffer_Serial[0] = 0; // serial data processed, clear buffer
    }
  }
}

int EspEasy_Console_t::getRoomLeft() const {
  #ifdef USE_SECOND_HEAP

  // If stored in 2nd heap, we must check this for space
  HeapSelectIram ephemeral;
  #endif // ifdef USE_SECOND_HEAP

  int roomLeft = getMaxFreeBlock();

  if (roomLeft < 1000) {
    roomLeft = 0;                               // Do not append to buffer.
  } else if (roomLeft < 4000) {
    roomLeft = 128 - _serialWriteBuffer.size(); // 1 buffer.
  } else {
    roomLeft -= 4000;                           // leave some free for normal use.
  }
  return roomLeft;
}

void EspEasy_Console_t::addToSerialBuffer(const __FlashStringHelper *line)
{
  addToSerialBuffer(String(line));
}

void EspEasy_Console_t::addToSerialBuffer(char c)
{
  addToSerialBuffer(String(c));
}

void EspEasy_Console_t::addToSerialBuffer(const String& line) {
  // When the buffer is too full, try to dump at least the size of what we try to add.
  const bool mustPop = !process_serialWriteBuffer() && _serialWriteBuffer.size() > 10000;
  {
    #ifdef USE_SECOND_HEAP

    // Allow to store the logs in 2nd heap if present.
    HeapSelectIram ephemeral;
    #endif // ifdef USE_SECOND_HEAP
    int roomLeft = getRoomLeft();

    auto it = line.begin();

    while (roomLeft > 0 && it != line.end()) {
      if (mustPop) {
        _serialWriteBuffer.pop_front();
      }
      _serialWriteBuffer.push_back(*it);
      --roomLeft;
      ++it;
    }
  }
  process_serialWriteBuffer();
}

void EspEasy_Console_t::addNewlineToSerialBuffer() {
  addToSerialBuffer(F("\r\n"));
}

bool EspEasy_Console_t::process_serialWriteBuffer() {
  const size_t bufferSize = _serialWriteBuffer.size();

  if (bufferSize == 0) {
    return true;
  }
  Stream *port = getPort();

  if (port == nullptr) {
    return false;
  }

  const size_t snip = availableForWrite();

  if (snip > 0) {
    size_t bytes_to_write = bufferSize;

    if (snip < bytes_to_write) { bytes_to_write = snip; }

    while (bytes_to_write > 0 && !_serialWriteBuffer.empty()) {
      const char c = _serialWriteBuffer.front();

      if (Settings.UseSerial) {
        port->write(c);
      }
      _serialWriteBuffer.pop_front();
      --bytes_to_write;
    }
  }

  return bufferSize != _serialWriteBuffer.size();
}

void EspEasy_Console_t::setDebugOutput(bool enable)
{
  if (_defaultPortActive) {
    _serial->setDebugOutput(enable);
  }
}

Stream * EspEasy_Console_t::getPort()
{
  if (_defaultPortActive) {
    return _serial;
  }
    #if CONSOLE_USES_HWCDC
  return &_hwcdc_serial;
    #elif CONSOLE_USES_USBCDC
  return &_usbcdc_serial;
    #else // if CONSOLE_USES_HWCDC
  return nullptr;
    #endif // if CONSOLE_USES_HWCDC
}

const Stream * EspEasy_Console_t::getPort() const
{
  if (_defaultPortActive) {
    return _serial;
  }
    #if CONSOLE_USES_HWCDC
  return &_hwcdc_serial;
    #elif CONSOLE_USES_USBCDC
  return &_usbcdc_serial;
    #else // if CONSOLE_USES_HWCDC
  return nullptr;
    #endif // if CONSOLE_USES_HWCDC
}

size_t EspEasy_Console_t::availableForWrite()
{
  if (_defaultPortActive) {
    if (_serial != nullptr) {
      return _serial->availableForWrite();
    }
    return 0;
  }
    #if CONSOLE_USES_HWCDC
  return _hwcdc_serial.availableForWrite();
    #elif CONSOLE_USES_USBCDC
  return _usbcdc_serial.availableForWrite();
    #else // if CONSOLE_USES_HWCDC
  return 0;
    #endif // if CONSOLE_USES_HWCDC
}


