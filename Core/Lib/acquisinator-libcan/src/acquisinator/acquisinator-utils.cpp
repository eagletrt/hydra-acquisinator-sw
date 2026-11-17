#include "acquisinator/acquisinator-utils.hpp"

int acquisinator_fields_string_from_id(enum CanAcquisinatorMessageFrameId id,
                                       char **v, size_t fields_size,
                                       size_t string_size) {
  switch (id) {

  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW:
    if (fields_size < 1)
      return 1;

    snprintf(v[0], string_size, ACQUISINATOR_RAW_DEBUG_ADC_RAW_U);
    return 0;
  default:
    break;
  }
  return 0;
}
int acquisinator_enum_fields(int enum_id, char **v, size_t fields_size,
                             size_t string_size) {
  switch (enum_id) {
  default:
    break;
  }
  return 0;
}

int acquisinator_serialize_from_string(enum CanAcquisinatorMessageFrameId id,
                                       char *s, uint8_t *data, size_t *size) {
  switch (id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW: {
    union CanAcquisinatorMessage tmp;
    uint16_t r_debug_adc_raw_u;

    sscanf(s, "%" SCNu16 ",", &r_debug_adc_raw_u);

    tmp.acquisinator_raw.debug_adc_raw_u = (uint16_t)r_debug_adc_raw_u;
    *size = (size_t)can_acquisinator_byte_size_acquisinator_raw;
    return can_acquisinator_api_serialize_from_index(
        CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW, &tmp, data);
  }
  default:
    break;
  }

  return 0;
}

int acquisinator_n_fields_from_id(enum CanAcquisinatorMessageFrameId id) {
  switch (id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW:
    return 1;
  default:
    break;
  }

  return 0;
}
int acquisinator_fields_types_from_id(enum CanAcquisinatorMessageFrameId id,
                                      int *fields_types,
                                      int fields_types_size) {
  switch (id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW:
    if (fields_types_size < 1)
      return 0;
    fields_types[0] = e_acquisinator_uint16_t;
    return 1;
  default:
    break;
  }
  return 0;
}

std::vector<std::string>
acquisinator_enum_fields_from_message(const std::string &msg_name) {
  std::vector<std::string> ret;

  return ret;
}
std::vector<std::string>
acquisinator_enum_names_from_fields(const std::string &msg_name,
                                    const std::string &sgn_name) {
  std::vector<std::string> ret;

  return ret;
}

int acquisinator_acquisinator_raw_to_string(
    struct CanAcquisinatorAcquisinatorRaw message, char *buffer) {
  return sprintf(buffer,
                 ""
#ifdef LIBCAN_TIMESTAMP
                 "%" PRIu64 ","
#endif // LIBCAN_TIMESTAMP

                 "%" PRIu16 "",

#ifdef LIBCAN_TIMESTAMP
                 message.timestamp,
#endif // LIBCAN_TIMESTAMP

                 message.debug_adc_raw_u

  );
}

int acquisinator_acquisinator_raw_to_string_file(
    struct CanAcquisinatorAcquisinatorRaw message, FILE *buffer) {
  return fprintf(buffer,
                 ""
#ifdef LIBCAN_TIMESTAMP
                 "%" PRIu64 ","
#endif // LIBCAN_TIMESTAMP

                 "%" PRIu16 "",

#ifdef LIBCAN_TIMESTAMP
                 message.timestamp,
#endif // LIBCAN_TIMESTAMP

                 message.debug_adc_raw_u

  );
}

int acquisinator_acquisinator_raw_fields(char *buffer) {
  return sprintf(buffer, ""
#ifdef LIBCAN_TIMESTAMP
                         "timestamp"
                         ","
#endif // LIBCAN_TIMESTAMP

                         "debug_adc_raw_u"

  );
}

int acquisinator_acquisinator_raw_fields_file(FILE *buffer) {
  return fprintf(buffer, ""
#ifdef LIBCAN_TIMESTAMP
                         "timestamp"
                         ","
#endif // LIBCAN_TIMESTAMP

                         "debug_adc_raw_u"

  );
}

int acquisinator_to_string_from_id(
    enum CanAcquisinatorMessageFrameId message_id,
    union CanAcquisinatorMessage *messages, char *buffer) {
  switch (message_id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW:
    return acquisinator_acquisinator_raw_to_string(messages->acquisinator_raw,
                                                   buffer);
  default:
    break;
  }
  return -1;
}

int acquisinator_to_string_file_from_id(
    enum CanAcquisinatorMessageFrameId message_id,
    union CanAcquisinatorMessage *messages, FILE *buffer) {
  switch (message_id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW:
    return acquisinator_acquisinator_raw_to_string_file(
        messages->acquisinator_raw, buffer);
  default:
    break;
  }
  return -1;
}

int acquisinator_fields_file_from_id(
    enum CanAcquisinatorMessageFrameId message_id, FILE *buffer) {
  switch (message_id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW:
    return acquisinator_acquisinator_raw_fields_file(buffer);
  default:
    break;
  }
  return -1;
}

int acquisinator_fields_from_id(enum CanAcquisinatorMessageFrameId message_id,
                                char *buffer) {
  switch (message_id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW:
    return acquisinator_acquisinator_raw_fields(buffer);
  default:
    break;
  }
  return -1;
}

int acquisinator_message_name_from_id(
    enum CanAcquisinatorMessageFrameId message_id, char *buffer) {
  switch (message_id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW:
    return sprintf(buffer, "%s", "acquisinator_raw");
  default:
    break;
  }
  return 0;
}