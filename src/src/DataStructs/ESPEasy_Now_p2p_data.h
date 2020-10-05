#ifndef DATASTRUCTS_ESPEASY_NOW_P2P_DATA_H
#define DATASTRUCTS_ESPEASY_NOW_P2P_DATA_H

#include <Arduino.h>

#include "ESPEasyLimits.h"
#include "../Globals/Plugins.h"

#define ESPEASY_NOW_P2P_DATA_VERSION    1

enum class ESPEasy_Now_p2p_data_type : uint8_t {
  NotSet            = 0,
  PluginDescription = 1,
  PluginData        = 2
};

struct __attribute__((__packed__)) ESPEasy_Now_p2p_data {

  ~ESPEasy_Now_p2p_data();

  // Add float at the end of the binary data
  // @param value  The float to store
  // @retval       True when successful. Only fails if no data could be allocated
  bool addFloat(float value);

  // Get a float, starting at @offset in the data array
  // @param value   The value to be read
  // @param offset  The offset in the data array to start. Will be incremented with sizeof(float)
  // @retval        True when the offset was in the range of the data.
  bool getFloat(float & value,
                size_t& offset) const;

  // Return the size of the header + data
  size_t getTotalSize() const;

  bool   addBinaryData(uint8_t *binaryData,
                       size_t   size);

  // Get a pointer to the data starting at offset
  // @param offset is the offset in the data array to start (will not be updated)
  // @param size is the expected size of the data. Value may be lower if there is less data available than requested.
  // @retval  Pointer to the data, if available. nullptr if offset is not valid.
  const uint8_t* getBinaryData(size_t  offset,
                               size_t& size) const;

  // Clear and resize data array to size.
  // @retval pointer to the first element of the data array
  uint8_t* prepareBinaryData(size_t& size);

private:

  // Allocate extra data at the end of the data array
  // @param size   The extra size in bytes to allocate
  // @retval  The old size of the data array.
  bool allocate(size_t  size,
                size_t& oldSize);

public:

  ESPEasy_Now_p2p_data_type dataType        = ESPEasy_Now_p2p_data_type::NotSet;
  uint16_t                  dataSize        = 0;

  // The sizeof(data) is intended use of sizeof to return the size of a pointer
  const uint8_t             dataOffset      = sizeof(ESPEasy_Now_p2p_data) - sizeof(data);
  taskIndex_t               sourceTaskIndex = INVALID_TASK_INDEX;
  taskIndex_t               destTaskIndex   = INVALID_TASK_INDEX;
  uint16_t                  plugin_id       = INVALID_PLUGIN_ID; // FIXME TD-er: Must change to pluginID_t as soon as that's changed to 16
                                                                 // bit
  uint16_t                  sourceUnit      = 0;
  uint16_t                  destUnit        = 0;

private:

  // Must use a pointer here instead of a struct, or else this object cannot be made packed.
  uint8_t *data = nullptr;
};


#endif // DATASTRUCTS_ESPEASY_NOW_P2P_DATA_H
