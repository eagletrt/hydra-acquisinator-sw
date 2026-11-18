#include "acquisinator/acquisinator-utils.h"

int acquisinator_fields_string_from_id(enum CanAcquisinatorMessageFrameId id,
                                       char **v, size_t fields_size,
                                       size_t string_size) {
  switch (id) {

  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW:
    if (fields_size < 4)
      return 1;

    snprintf(v[0], string_size, ACQUISINATOR_RAW_DEBUG_ADC_RAW0_U);
    snprintf(v[1], string_size, ACQUISINATOR_RAW_DEBUG_ADC_RAW1_U);
    snprintf(v[2], string_size, ACQUISINATOR_RAW_DEBUG_ADC_RAW2_U);
    snprintf(v[3], string_size, ACQUISINATOR_RAW_DEBUG_SEQ_U);
    return 0;
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN:
    if (fields_size < 2)
      return 1;

    snprintf(v[0], string_size, ACQUISINATOR_LOCKIN_LOCKIN_INPHASE);
    snprintf(v[1], string_size, ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE);
    return 0;
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS:
    if (fields_size < 5)
      return 1;

    snprintf(v[0], string_size, ACQUISINATOR_LOCKIN_STATUS_LOCKIN_DC_U);
    snprintf(v[1], string_size, ACQUISINATOR_LOCKIN_STATUS_LOCKIN_MIN_U);
    snprintf(v[2], string_size, ACQUISINATOR_LOCKIN_STATUS_LOCKIN_MAX_U);
    snprintf(v[3], string_size, ACQUISINATOR_LOCKIN_STATUS_LOCKIN_SPI_ERROR_B);
    snprintf(v[4], string_size, ACQUISINATOR_LOCKIN_STATUS_LOCKIN_SEQ_U);
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
    uint16_t r_debug_adc_raw0_u;
    uint16_t r_debug_adc_raw1_u;
    uint16_t r_debug_adc_raw2_u;
    uint8_t r_debug_seq_u;

    sscanf(s,
           "%" SCNu16 ","
           "%" SCNu16 ","
           "%" SCNu16 ","
           "%" SCNu8 ",",
           &r_debug_adc_raw0_u, &r_debug_adc_raw1_u, &r_debug_adc_raw2_u,
           &r_debug_seq_u);

    tmp.acquisinator_raw.debug_adc_raw0_u = (uint16_t)r_debug_adc_raw0_u;
    tmp.acquisinator_raw.debug_adc_raw1_u = (uint16_t)r_debug_adc_raw1_u;
    tmp.acquisinator_raw.debug_adc_raw2_u = (uint16_t)r_debug_adc_raw2_u;
    tmp.acquisinator_raw.debug_seq_u = (uint8_t)r_debug_seq_u;
    *size = (size_t)can_acquisinator_byte_size_acquisinator_raw;
    return can_acquisinator_api_serialize_from_index(
        CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW, &tmp, data);
  }
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN: {
    union CanAcquisinatorMessage tmp;
    float r_lockin_inphase;
    float r_lockin_quadrature;

    sscanf(s,
           "%f,"
           "%f,",
           &r_lockin_inphase, &r_lockin_quadrature);

    tmp.acquisinator_lockin.lockin_inphase = (float)r_lockin_inphase;
    tmp.acquisinator_lockin.lockin_quadrature = (float)r_lockin_quadrature;
    *size = (size_t)can_acquisinator_byte_size_acquisinator_lockin;
    return can_acquisinator_api_serialize_from_index(
        CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN, &tmp, data);
  }
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS: {
    union CanAcquisinatorMessage tmp;
    uint16_t r_lockin_dc_u;
    uint16_t r_lockin_min_u;
    uint16_t r_lockin_max_u;
    bool r_lockin_spi_error_b;
    uint8_t r_lockin_seq_u;

    sscanf(s,
           "%" SCNu16 ","
           "%" SCNu16 ","
           "%" SCNu16 ","
           "%" SCNu8 ","
           "%" SCNu8 ",",
           &r_lockin_dc_u, &r_lockin_min_u, &r_lockin_max_u,
           &r_lockin_spi_error_b, &r_lockin_seq_u);

    tmp.acquisinator_lockin_status.lockin_dc_u = (uint16_t)r_lockin_dc_u;
    tmp.acquisinator_lockin_status.lockin_min_u = (uint16_t)r_lockin_min_u;
    tmp.acquisinator_lockin_status.lockin_max_u = (uint16_t)r_lockin_max_u;
    tmp.acquisinator_lockin_status.lockin_spi_error_b =
        (bool)r_lockin_spi_error_b;
    tmp.acquisinator_lockin_status.lockin_seq_u = (uint8_t)r_lockin_seq_u;
    *size = (size_t)can_acquisinator_byte_size_acquisinator_lockin_status;
    return can_acquisinator_api_serialize_from_index(
        CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS, &tmp, data);
  }
  default:
    break;
  }

  return 0;
}

int acquisinator_n_fields_from_id(enum CanAcquisinatorMessageFrameId id) {
  switch (id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW:
    return 4;
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN:
    return 2;
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS:
    return 5;
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
    if (fields_types_size < 4)
      return 0;
    fields_types[0] = e_acquisinator_uint16_t;
    fields_types[1] = e_acquisinator_uint16_t;
    fields_types[2] = e_acquisinator_uint16_t;
    fields_types[3] = e_acquisinator_uint8_t;
    return 4;
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN:
    if (fields_types_size < 2)
      return 0;
    fields_types[0] = e_acquisinator_float;
    fields_types[1] = e_acquisinator_float;
    return 2;
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS:
    if (fields_types_size < 5)
      return 0;
    fields_types[0] = e_acquisinator_uint16_t;
    fields_types[1] = e_acquisinator_uint16_t;
    fields_types[2] = e_acquisinator_uint16_t;
    fields_types[3] = e_acquisinator_bool;
    fields_types[4] = e_acquisinator_uint8_t;
    return 5;
  default:
    break;
  }
  return 0;
}
bool acquisinator_id_is_message(enum CanAcquisinatorMessageFrameId id) {
  switch (id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW:
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN:
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS:
    return true;
  default:
    return false;
  }
}

int acquisinator_acquisinator_raw_to_string(
    struct CanAcquisinatorAcquisinatorRaw message, char *buffer) {
  return sprintf(buffer,
                 ""
#ifdef LIBCAN_TIMESTAMP
                 "%" PRIu64 ","
#endif // LIBCAN_TIMESTAMP
                 "%" PRIu16 ""
                 ","
                 "%" PRIu16 ""
                 ","
                 "%" PRIu16 ""
                 ","
                 "%" PRIu8 "",

#ifdef LIBCAN_TIMESTAMP
                 message.timestamp,
#endif // LIBCAN_TIMESTAMP

                 message.debug_adc_raw0_u, message.debug_adc_raw1_u,
                 message.debug_adc_raw2_u, message.debug_seq_u

  );
}

int acquisinator_acquisinator_raw_to_string_file(
    struct CanAcquisinatorAcquisinatorRaw message, FILE *buffer) {
  return fprintf(buffer,
                 ""
#ifdef LIBCAN_TIMESTAMP
                 "%" PRIu64 ","
#endif // LIBCAN_TIMESTAMP

                 "%" PRIu16 ""
                 ","
                 "%" PRIu16 ""
                 ","
                 "%" PRIu16 ""
                 ","
                 "%" PRIu8 "",

#ifdef LIBCAN_TIMESTAMP
                 message.timestamp,
#endif // LIBCAN_TIMESTAMP

                 message.debug_adc_raw0_u, message.debug_adc_raw1_u,
                 message.debug_adc_raw2_u, message.debug_seq_u

  );
}

int acquisinator_acquisinator_raw_fields(char *buffer) {
  return sprintf(buffer, ""
#ifdef LIBCAN_TIMESTAMP
                         "timestamp"
                         ","
#endif // LIBCAN_TIMESTAMP

                         "debug_adc_raw0_u"
                         ","
                         "debug_adc_raw1_u"
                         ","
                         "debug_adc_raw2_u"
                         ","
                         "debug_seq_u"

  );
}

int acquisinator_acquisinator_raw_fields_file(FILE *buffer) {
  return fprintf(buffer, ""
#ifdef LIBCAN_TIMESTAMP
                         "timestamp"
                         ","
#endif // LIBCAN_TIMESTAMP

                         "debug_adc_raw0_u"
                         ","
                         "debug_adc_raw1_u"
                         ","
                         "debug_adc_raw2_u"
                         ","
                         "debug_seq_u"

  );
}

int acquisinator_acquisinator_lockin_to_string(
    struct CanAcquisinatorAcquisinatorLockin message, char *buffer) {
  return sprintf(buffer,
                 ""
#ifdef LIBCAN_TIMESTAMP
                 "%" PRIu64 ","
#endif // LIBCAN_TIMESTAMP
                 "%f"
                 ""
                 ","
                 "%f"
                 "",

#ifdef LIBCAN_TIMESTAMP
                 message.timestamp,
#endif // LIBCAN_TIMESTAMP

                 message.lockin_inphase, message.lockin_quadrature

  );
}

int acquisinator_acquisinator_lockin_to_string_file(
    struct CanAcquisinatorAcquisinatorLockin message, FILE *buffer) {
  return fprintf(buffer,
                 ""
#ifdef LIBCAN_TIMESTAMP
                 "%" PRIu64 ","
#endif // LIBCAN_TIMESTAMP

                 "%f"
                 ""
                 ","
                 "%f"
                 "",

#ifdef LIBCAN_TIMESTAMP
                 message.timestamp,
#endif // LIBCAN_TIMESTAMP

                 message.lockin_inphase, message.lockin_quadrature

  );
}

int acquisinator_acquisinator_lockin_fields(char *buffer) {
  return sprintf(buffer, ""
#ifdef LIBCAN_TIMESTAMP
                         "timestamp"
                         ","
#endif // LIBCAN_TIMESTAMP

                         "lockin_inphase"
                         ","
                         "lockin_quadrature"

  );
}

int acquisinator_acquisinator_lockin_fields_file(FILE *buffer) {
  return fprintf(buffer, ""
#ifdef LIBCAN_TIMESTAMP
                         "timestamp"
                         ","
#endif // LIBCAN_TIMESTAMP

                         "lockin_inphase"
                         ","
                         "lockin_quadrature"

  );
}

int acquisinator_acquisinator_lockin_status_to_string(
    struct CanAcquisinatorAcquisinatorLockinStatus message, char *buffer) {
  return sprintf(buffer,
                 ""
#ifdef LIBCAN_TIMESTAMP
                 "%" PRIu64 ","
#endif // LIBCAN_TIMESTAMP
                 "%" PRIu16 ""
                 ","
                 "%" PRIu16 ""
                 ","
                 "%" PRIu16 ""
                 ","
                 "%" PRIu8 ""
                 ","
                 "%" PRIu8 "",

#ifdef LIBCAN_TIMESTAMP
                 message.timestamp,
#endif // LIBCAN_TIMESTAMP

                 message.lockin_dc_u, message.lockin_min_u,
                 message.lockin_max_u, message.lockin_spi_error_b,
                 message.lockin_seq_u

  );
}

int acquisinator_acquisinator_lockin_status_to_string_file(
    struct CanAcquisinatorAcquisinatorLockinStatus message, FILE *buffer) {
  return fprintf(buffer,
                 ""
#ifdef LIBCAN_TIMESTAMP
                 "%" PRIu64 ","
#endif // LIBCAN_TIMESTAMP

                 "%" PRIu16 ""
                 ","
                 "%" PRIu16 ""
                 ","
                 "%" PRIu16 ""
                 ","
                 "%" PRIu8 ""
                 ","
                 "%" PRIu8 "",

#ifdef LIBCAN_TIMESTAMP
                 message.timestamp,
#endif // LIBCAN_TIMESTAMP

                 message.lockin_dc_u, message.lockin_min_u,
                 message.lockin_max_u, message.lockin_spi_error_b,
                 message.lockin_seq_u

  );
}

int acquisinator_acquisinator_lockin_status_fields(char *buffer) {
  return sprintf(buffer, ""
#ifdef LIBCAN_TIMESTAMP
                         "timestamp"
                         ","
#endif // LIBCAN_TIMESTAMP

                         "lockin_dc_u"
                         ","
                         "lockin_min_u"
                         ","
                         "lockin_max_u"
                         ","
                         "lockin_spi_error_b"
                         ","
                         "lockin_seq_u"

  );
}

int acquisinator_acquisinator_lockin_status_fields_file(FILE *buffer) {
  return fprintf(buffer, ""
#ifdef LIBCAN_TIMESTAMP
                         "timestamp"
                         ","
#endif // LIBCAN_TIMESTAMP

                         "lockin_dc_u"
                         ","
                         "lockin_min_u"
                         ","
                         "lockin_max_u"
                         ","
                         "lockin_spi_error_b"
                         ","
                         "lockin_seq_u"

  );
}

int acquisinator_to_string_from_id(
    enum CanAcquisinatorMessageFrameId message_id,
    union CanAcquisinatorMessage *messages, char *buffer) {
  switch (message_id) {
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW:
    return acquisinator_acquisinator_raw_to_string(messages->acquisinator_raw,
                                                   buffer);
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN:
    return acquisinator_acquisinator_lockin_to_string(
        messages->acquisinator_lockin, buffer);
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS:
    return acquisinator_acquisinator_lockin_status_to_string(
        messages->acquisinator_lockin_status, buffer);
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
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN:
    return acquisinator_acquisinator_lockin_to_string_file(
        messages->acquisinator_lockin, buffer);
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS:
    return acquisinator_acquisinator_lockin_status_to_string_file(
        messages->acquisinator_lockin_status, buffer);
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
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN:
    return acquisinator_acquisinator_lockin_fields_file(buffer);
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS:
    return acquisinator_acquisinator_lockin_status_fields_file(buffer);
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
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN:
    return acquisinator_acquisinator_lockin_fields(buffer);
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS:
    return acquisinator_acquisinator_lockin_status_fields(buffer);
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
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN:
    return sprintf(buffer, "%s", "acquisinator_lockin");
  case CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS:
    return sprintf(buffer, "%s", "acquisinator_lockin_status");
  default:
    break;
  }
  return 0;
}