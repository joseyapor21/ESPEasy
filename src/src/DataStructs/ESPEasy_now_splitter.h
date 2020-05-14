#ifndef DATASTRUCTS_ESPEASY_NOW_SPLITTER_H
#define DATASTRUCTS_ESPEASY_NOW_SPLITTER_H

#include "ESPEasy_Now_packet.h"

#ifdef USES_ESPEASY_NOW

class ESPEasy_now_splitter {
public:

  ESPEasy_now_splitter(ESPEasy_now_hdr::message_t message_type, size_t totalSize);

  size_t addBinaryData(const uint8_t *data,
                       size_t   length);

  size_t addString(const String& string);

  bool   send(uint8_t mac[6]);

private:

  // Create next packet when needed.
  void createNextPacket(size_t data_left);

  size_t  getPayloadPos() const;

  void                 setMac(uint8_t mac[6]);

  bool                 send(const ESPEasy_Now_packet& packet);
  WifiEspNowSendStatus send(const ESPEasy_Now_packet& packet,
                            size_t                    timeout);

  WifiEspNowSendStatus waitForSendStatus(size_t timeout) const;

  std::vector<ESPEasy_Now_packet> _queue;
  ESPEasy_now_hdr _header;
  size_t _payload_pos = 255;     // Position in the last packet where we left of.
  const size_t _totalSize = 0;   // Total size as we intend to send.
  size_t _bytesStored = 0;       // Total number of bytes already stored as payload

};

#endif // ifdef USES_ESPEASY_NOW

#endif // DATASTRUCTS_ESPEASY_NOW_SPLITTER_H
