#ifndef PLUGINSTRUCTS_P094_DATA_STRUCT_H
#define PLUGINSTRUCTS_P094_DATA_STRUCT_H

#include "../../_Plugin_Helper.h"
#ifdef USES_P094

# include "../Helpers/CUL_interval_filter.h"
# include "../Helpers/CUL_stats.h"


# include <ESPeasySerial.h>
# include <Regexp.h>

# ifndef P094_DEBUG_OPTIONS
#  define P094_DEBUG_OPTIONS 0
# endif // ifndef P094_DEBUG_OPTIONS


# define P094_REGEX_POS             0
# define P094_NR_CHAR_USE_POS       1
# define P094_FILTER_OFF_WINDOW_POS 2
# define P094_MATCH_TYPE_POS        3

# define P094_FIRST_FILTER_POS   10

# define P094_ITEMS_PER_FILTER   4
# define P094_AND_FILTER_BLOCK   3
# define P094_NR_FILTERS         (7 * P094_AND_FILTER_BLOCK)
# define P94_Nlines              (P094_FIRST_FILTER_POS + (P094_ITEMS_PER_FILTER * (P094_NR_FILTERS)))
# define P94_Nchars              128
# define P94_MAX_CAPTURE_INDEX   32


enum P094_Match_Type {
  P094_Regular_Match          = 0,
  P094_Regular_Match_inverted = 1,
  P094_Filter_Disabled        = 2
};
# define P094_Match_Type_NR_ELEMENTS 3

enum P094_Filter_Value_Type {
  P094_not_used      = 0,
  P094_packet_length = 1,
  P094_unknown1      = 2,
  P094_manufacturer  = 3,
  P094_serial_number = 4,
  P094_unknown2      = 5,
  P094_meter_type    = 6,
  P094_rssi          = 7,
  P094_position      = 8
};
# define P094_FILTER_VALUE_Type_NR_ELEMENTS 9

enum P094_Filter_Comp {
  P094_Equal_OR      = 0,
  P094_NotEqual_OR   = 1,
  P094_Equal_MUST    = 2,
  P094_NotEqual_MUST = 3
};

# define P094_FILTER_COMP_NR_ELEMENTS 4

enum class P094_Filter_Window {
  All,             // Realtime, every message passes the filter
  Five_minutes,    // a message passes the filter every 5 minutes, aligned to time (00:00:00, 00:05:00, ...)
  Fifteen_minutes, // a message passes the filter every 15 minutes, aligned to time
  One_hour,        // a message passes the filter every hour, aligned to time
  Day,             // a message passes the filter once every day
                   // - between 00:00 and 12:00,
                   // - between 12:00 and 23:00 and
                   // - between 23:00 and 00:00
  Month,           // a message passes the filter
                   // - between 1st of month 00:00:00 and 15th of month 00:00:00
                   // - between 15th of month 00:00:00 and last of month 00:00:00
                   // - between last of month 00:00:00 and 1st of next month 00:00:00
  Once,            // only one message passes the filter until next reboot
  None             // no messages pass the filter
};




// Examples for a filter definition list
//   EBZ.02.12345678;all
//   *.02.*;15m
//   TCH.44.*;Once
//   !TCH.*.*;None
//   *.*.*;5m

struct P094_filter {

  void fromString(const String& str);
  String toString() const;

  // Check to see if the manufacturer, metertype and serial matches.
  bool matches(const mBusPacket_header_t& other) const;

  // Check against the filter window.
  bool shouldPass();


  unsigned long _lastSeenUnixTime{};

  mBusPacket_header_t _header;
  P094_Filter_Window _filterWindow = P094_Filter_Window::All;
  P094_Filter_Comp   _filterComp = P094_Filter_Comp::P094_Equal_OR;
};

struct P094_data_struct : public PluginTaskData_base {
public:


  P094_data_struct();

  virtual ~P094_data_struct();

  void reset();

  bool init(ESPEasySerialPort port,
            const int16_t     serial_rx,
            const int16_t     serial_tx,
            unsigned long     baudrate,
            bool              intervalFilterEnabled,
            bool              collectStats);

  void          post_init();

  bool          isInitialized() const;

  void          sendString(const String& data);

  bool          loop();

  const String& peekSentence() const;

  void          getSentence(String& string,
                            bool    appendSysTime);

  void          getSentencesReceived(uint32_t& succes,
                                     uint32_t& error,
                                     uint32_t& length_last) const;

  void setMaxLength(uint16_t maxlenght);

  void setLine(uint8_t       varNr,
               const String& line);


  uint32_t        getFilterOffWindowTime() const;

  P094_Match_Type getMatchType() const;

  bool            invertMatch() const;

  bool            filterUsed(uint8_t lineNr) const;

  String          getFilter(uint8_t                 lineNr,
                            P094_Filter_Value_Type& capture,
                            uint32_t              & optional,
                            P094_Filter_Comp      & comparator) const;

  void                              setDisableFilterWindowTimer();

  bool                              disableFilterWindowActive() const;

  bool                              parsePacket(const String& received,
                                                mBusPacket_t& packet) const;

  static const __FlashStringHelper* MatchType_toString(P094_Match_Type matchType);
  static const __FlashStringHelper* P094_FilterValueType_toString(P094_Filter_Value_Type valueType);
  static const __FlashStringHelper* P094_FilterComp_toString(P094_Filter_Comp comparator);


  // Made public so we don't have to copy the values when loading/saving.
  String _lines[P94_Nlines];

  static size_t P094_Get_filter_base_index(size_t filterLine);

# if P094_DEBUG_OPTIONS

  // Get (and increment) debug counter
  uint32_t getDebugCounter();

  void     setGenerate_DebugCulData(bool value) {
    debug_generate_CUL_data = value;
  }

# endif // if P094_DEBUG_OPTIONS

  bool interval_filter_add(const mBusPacket_t& packet);
  void interval_filter_purgeExpired();

  bool collect_stats_add(const mBusPacket_t& packet);
  void prepare_dump_stats();
  bool dump_next_stats(String& str);

private:

  bool max_length_reached() const;

  ESPeasySerial *easySerial = nullptr;
  String         sentence_part;
  uint16_t       max_length               = 550;
  uint32_t       sentences_received       = 0;
  uint32_t       sentences_received_error = 0;
  bool           current_sentence_errored = false;
  uint32_t       length_last_received     = 0;
  unsigned long  disable_filter_window    = 0;
  # if P094_DEBUG_OPTIONS
  uint32_t debug_counter           = 0;
  bool     debug_generate_CUL_data = false;
  # endif // if P094_DEBUG_OPTIONS
  bool interval_filter_enabled = false;
  bool collect_stats           = false;

  bool firstStatsIndexActive = false;

  bool                   filterValueType_used[P094_FILTER_VALUE_Type_NR_ELEMENTS] = { 0 };
  P094_Filter_Value_Type filterLine_valueType[P094_NR_FILTERS];
  P094_Filter_Comp       filterLine_compare[P094_NR_FILTERS];

  CUL_interval_filter interval_filter;

  // Alternating stats, one being flushed, the other used to collect new stats
  CUL_Stats mBus_stats[2];
};


#endif // USES_P094

#endif // PLUGINSTRUCTS_P094_DATA_STRUCT_H
