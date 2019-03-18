/*********************************************************************************************\
 * RTC memory stored values
\*********************************************************************************************/

/*
  During deep sleep, only RTC still working, so maybe we need to save some user data in RTC memory.
  Only user data area can be used by user.
  |<--------system data--------->|<-----------------user data--------------->|
  | 256 bytes                    | 512 bytes                                 |
  Note:
  RTC memory is 4 bytes aligned for read and write operations.
  Address parameter refers to block number(4 bytes per block).
  So, if we want to access some data at the beginning of user data area,
  address: 256/4 = 64
  data   : data pointer
  size   : data length, byte

  Prototype:
    bool system_rtc_mem_read (
      uint32 src_addr,
      void * data,
      uint32 save_size
    )

    bool system_rtc_mem_write (
      uint32 des_addr,
      void * data,
      uint32 save_size
    )
*/

// RTC layout ESPeasy:
// these offsets are in blocks, bytes = blocks * 4
// 64   RTCStruct  max 40 bytes: ( 74 - 64 ) * 4
// 74   UserVar
// 122  UserVar checksum:  RTC_BASE_USERVAR + (sizeof(UserVar) / 4)
// 128  Cache (C014) metadata  4 blocks
// 132  Cache (C014) data  6 blocks per sample => max 10 samples




/********************************************************************************************\
  Save RTC struct to RTC memory
  \*********************************************************************************************/
boolean saveToRTC()
{
  #if defined(ESP32)
    return false;
  #else
    if (!system_rtc_mem_write(RTC_BASE_STRUCT, (byte*)&RTC, sizeof(RTC)) || !readFromRTC())
    {
      addLog(LOG_LEVEL_ERROR, F("RTC  : Error while writing to RTC"));
      return(false);
    }
    else
    {
      return(true);
    }
  #endif
}


/********************************************************************************************\
  Initialize RTC memory
  \*********************************************************************************************/
void initRTC()
{
  memset(&RTC, 0, sizeof(RTC));
  RTC.ID1 = 0xAA;
  RTC.ID2 = 0x55;
  saveToRTC();

  memset(&UserVar, 0, sizeof(UserVar));
  saveUserVarToRTC();
}

/********************************************************************************************\
  Read RTC struct from RTC memory
  \*********************************************************************************************/
boolean readFromRTC()
{
  #if defined(ESP32)
    return false;
  #else
    if (!system_rtc_mem_read(RTC_BASE_STRUCT, (byte*)&RTC, sizeof(RTC)))
      return(false);
    return (RTC.ID1 == 0xAA && RTC.ID2 == 0x55);
  #endif
}


/********************************************************************************************\
  Save values to RTC memory
\*********************************************************************************************/
boolean saveUserVarToRTC()
{
  #if defined(ESP32)
    return false;
  #else
    //addLog(LOG_LEVEL_DEBUG, F("RTCMEM: saveUserVarToRTC"));
    byte* buffer = (byte*)&UserVar;
    size_t size = sizeof(UserVar);
    uint32_t sum = calc_CRC32(buffer, size);
    boolean ret = system_rtc_mem_write(RTC_BASE_USERVAR, buffer, size);
    ret &= system_rtc_mem_write(RTC_BASE_USERVAR+(size>>2), (byte*)&sum, 4);
    return ret;
  #endif
}


/********************************************************************************************\
  Read RTC struct from RTC memory
\*********************************************************************************************/
boolean readUserVarFromRTC()
{
  #if defined(ESP32)
    return false;
  #else
    //addLog(LOG_LEVEL_DEBUG, F("RTCMEM: readUserVarFromRTC"));
    byte* buffer = (byte*)&UserVar;
    size_t size = sizeof(UserVar);
    boolean ret = system_rtc_mem_read(RTC_BASE_USERVAR, buffer, size);
    uint32_t sumRAM = calc_CRC32(buffer, size);
    uint32_t sumRTC = 0;
    ret &= system_rtc_mem_read(RTC_BASE_USERVAR+(size>>2), (byte*)&sumRTC, 4);
    if (!ret || sumRTC != sumRAM)
    {
      addLog(LOG_LEVEL_ERROR, F("RTC  : Checksum error on reading RTC user var"));
      memset(buffer, 0, size);
    }
    return ret;
  #endif
}


/********************************************************************************************\
  RTC located cache
\*********************************************************************************************/
struct RTC_cache_handler_struct
{
  RTC_cache_handler_struct() {
    bool success = loadMetaData() && loadData();
    if (!success) {
      addLog(LOG_LEVEL_INFO, F("RTC  : Error reading cache data"));
      memset(&RTC_cache, 0, sizeof(RTC_cache));
      flush();
    } else {
      rtc_debug_log(F("Read from RTC cache"), RTC_cache.writePos);
    }
  }

  unsigned int getFreeSpace() {
    if (RTC_cache.writePos >= RTC_CACHE_DATA_SIZE) {
      return 0;
    }
    return RTC_CACHE_DATA_SIZE - RTC_cache.writePos;
  }


  // Write a single sample set to the buffer
  bool write(uint8_t* data, unsigned int size) {
    rtc_debug_log(F("write RTC cache data"), size);
    if (getFreeSpace() < size) {
      if (!flush()) {
        return false;
      }
    }
    // First store it in the buffer
    for (unsigned int i = 0; i < size; ++i) {
      RTC_cache_data[RTC_cache.writePos] = data[i];
      ++RTC_cache.writePos;
    }

    // Now store the updated part of the buffer to the RTC memory.
    // Pad some extra bytes around it to allow sample sizes not multiple of 4 bytes.
    int startOffset = RTC_cache.writePos - size;
    startOffset -= startOffset % 4;
    if (startOffset < 0) {
      startOffset = 0;
    }
    int nrBytes = RTC_cache.writePos - startOffset;
    if (nrBytes % 4 != 0) {
      nrBytes -= nrBytes % 4;
      nrBytes += 4;
    }
    if ((nrBytes + startOffset) >  RTC_CACHE_DATA_SIZE) {
      // Can this happen?
      nrBytes = RTC_CACHE_DATA_SIZE - startOffset;
    }
    return saveRTCcache(startOffset, nrBytes);
  }

  // Mark all content as being processed and empty buffer.
  bool flush() {
    if (prepareFileForWrite()) {
      if (RTC_cache.writePos > 0) {
        if (fw.write(&RTC_cache_data[0], RTC_cache.writePos) < 0) {
          addLog(LOG_LEVEL_ERROR, F("RTC  : error writing file"));
          return false;
        }
        delay(0);
        fw.flush();

        addLog(LOG_LEVEL_INFO, F("RTC  : flush RTC cache"));
        initRTCcache_data();
        clearRTCcacheData();
        saveRTCcache();
        return true;
      }
    }
    return false;
  }

  // Return usable filename for reading.
  // Will be empty if there is no file to process.
  String getReadCacheFileName(int& readPos) {
    initRTCcache_data();
    for (int i=0; i < 2; ++i) {
      String fname = createCacheFilename(RTC_cache.readFileNr);
      if (SPIFFS.exists(fname)) {
        if (i != 0) {
          // First attempt failed, so stored read position is not valid
          RTC_cache.readPos = 0;
        }
        readPos = RTC_cache.readPos;
        return fname;
      }
      if (i == 0) {
        updateRTC_filenameCounters();
      }
    }
    // No file found
    RTC_cache.readPos = 0;
    readPos = RTC_cache.readPos;
    return "";
  }

private:

  bool loadMetaData()
  {
    #if defined(ESP32)
      return false;
    #else
      if (!system_rtc_mem_read(RTC_BASE_CACHE, (byte*)&RTC_cache, sizeof(RTC_cache)))
        return(false);

      return (RTC_cache.checksumMetadata == calc_CRC32((byte*)&RTC_cache, sizeof(RTC_cache) - sizeof(uint32_t)));
    #endif
  }

  bool loadData()
  {
    #if defined(ESP32)
      return false;
    #else
      initRTCcache_data();
      if (!system_rtc_mem_read(RTC_BASE_CACHE + (sizeof(RTC_cache) / 4), (byte*)&RTC_cache_data[0], RTC_CACHE_DATA_SIZE))
        return(false);

      if (RTC_cache.checksumData != getDataChecksum()) {
        addLog(LOG_LEVEL_ERROR, F("RTC  : Checksum error reading RTC cache data"));
        return(false);
      }
      return (RTC_cache.checksumData == getDataChecksum());
    #endif
  }

  bool saveRTCcache() {
    return saveRTCcache(0, RTC_CACHE_DATA_SIZE);
  }

  bool saveRTCcache(unsigned int startOffset, size_t nrBytes)
  {
    #if defined(ESP32)
      return false;
    #else
      RTC_cache.checksumData = getDataChecksum();
      RTC_cache.checksumMetadata = calc_CRC32((byte*)&RTC_cache, sizeof(RTC_cache) - sizeof(uint32_t));
      if (!system_rtc_mem_write(RTC_BASE_CACHE, (byte*)&RTC_cache, sizeof(RTC_cache)) || !loadMetaData())
      {
        addLog(LOG_LEVEL_ERROR, F("RTC  : Error while writing cache metadata to RTC"));
        return(false);
      }
      delay(0);
      if (nrBytes > 0) { // Check needed?
        const size_t address = RTC_BASE_CACHE + ((sizeof(RTC_cache) + startOffset) / 4);
        if (!system_rtc_mem_write(address, (byte*)&RTC_cache_data[startOffset], nrBytes))
        {
          addLog(LOG_LEVEL_ERROR, F("RTC  : Error while writing cache data to RTC"));
          return(false);
        }
        rtc_debug_log(F("Write cache data to RTC"), nrBytes);
      }
      return(true);
    #endif
  }

  uint32_t getDataChecksum() {
    initRTCcache_data();
    size_t dataLength = RTC_cache.writePos;
    if (dataLength > RTC_CACHE_DATA_SIZE) {
      // Is this allowed to happen?
      dataLength = RTC_CACHE_DATA_SIZE;
    }
    // Only compute the checksum over the number of samples stored.
    return calc_CRC32((byte*)&RTC_cache_data[0], RTC_CACHE_DATA_SIZE);
  }

  void initRTCcache_data() {
    if (RTC_cache_data.size() != RTC_CACHE_DATA_SIZE) {
      RTC_cache_data.resize(RTC_CACHE_DATA_SIZE);
    }
    if (RTC_cache.writeFileNr == 0) {
      // RTC value not reliable
      updateRTC_filenameCounters();
    }
  }

  void clearRTCcacheData() {
    for (size_t i = 0; i < RTC_CACHE_DATA_SIZE; ++i) {
      RTC_cache_data[i] = 0;
    }
    RTC_cache.writePos = 0;
  }

  void updateRTC_filenameCounters() {
    size_t filesizeHighest;
    if (getCacheFileCounters(RTC_cache.readFileNr, RTC_cache.writeFileNr, filesizeHighest)) {
      if (filesizeHighest >= CACHE_FILE_MAX_SIZE) {
        // Start new file
        ++RTC_cache.writeFileNr;
      }
    } else {
      // Do not use 0, since that will be the cleared content of the struct, indicating invalid RTC data.
      RTC_cache.writeFileNr = 1;
    }
  }

  bool prepareFileForWrite() {
    if (SpiffsFull()) {
      return false;
    }
    bool fileFound = false;
    unsigned int retries = 3;
    while (retries > 0) {
      --retries;
      if (fw && fw.size() >= CACHE_FILE_MAX_SIZE) {
        fw.close();
      }
      if (!fw) {
        // Open file to write
        initRTCcache_data();
        updateRTC_filenameCounters();
        String fname = createCacheFilename(RTC_cache.writeFileNr);
        fw = SPIFFS.open(fname.c_str(), "a+");
        if (!fw) {
          addLog(LOG_LEVEL_ERROR, F("RTC  : error opening file"));
          return false;
        }
        if (loglevelActiveFor(LOG_LEVEL_INFO)) {
          String log = F("Write to ");
          log += fname;
          log += F(" size");
          rtc_debug_log(log, fw.size());
        }
      }
      delay(0);
      if (fw && fw.size() < CACHE_FILE_MAX_SIZE) {
        return true;
      }
    }
    return false;
  }

  void rtc_debug_log(const String& description, size_t nrBytes) {
    if (loglevelActiveFor(LOG_LEVEL_INFO)) {
      String log;
      log.reserve(18 + description.length());
      log = F("RTC  : ");
      log += description;
      log += ' ';
      log += nrBytes;
      log += F(" bytes");
      addLog(LOG_LEVEL_INFO, log);
    }
  }

  RTC_cache_struct RTC_cache;
  std::vector<uint8_t> RTC_cache_data;
  File fw;

};
