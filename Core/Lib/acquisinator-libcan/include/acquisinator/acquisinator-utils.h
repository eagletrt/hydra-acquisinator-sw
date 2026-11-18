#ifndef acquisinator_UTILS_H
#define acquisinator_UTILS_H

#include <inttypes.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "acquisinator/can-acquisinator-api.h"

#ifdef __cplusplus
extern "C" {
#endif

/* START */
#define ACQUISINATOR_RAW "ACQUISINATOR_RAW"

#define ACQUISINATOR_RAW_DEBUG_ADC_RAW0_U "debug_adc_raw0_u"
#define ACQUISINATOR_RAW_DEBUG_ADC_RAW1_U "debug_adc_raw1_u"
#define ACQUISINATOR_RAW_DEBUG_ADC_RAW2_U "debug_adc_raw2_u"
#define ACQUISINATOR_RAW_DEBUG_SEQ_U "debug_seq_u"
/* END */

/* START */
#define ACQUISINATOR_LOCKIN "ACQUISINATOR_LOCKIN"

#define ACQUISINATOR_LOCKIN_LOCKIN_INPHASE "lockin_inphase"
#define ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE "lockin_quadrature"
/* END */

/* START */
#define ACQUISINATOR_LOCKIN_STATUS "ACQUISINATOR_LOCKIN_STATUS"

#define ACQUISINATOR_LOCKIN_STATUS_LOCKIN_DC_U "lockin_dc_u"
#define ACQUISINATOR_LOCKIN_STATUS_LOCKIN_MIN_U "lockin_min_u"
#define ACQUISINATOR_LOCKIN_STATUS_LOCKIN_MAX_U "lockin_max_u"
#define ACQUISINATOR_LOCKIN_STATUS_LOCKIN_SPI_ERROR_B "lockin_spi_error_b"
#define ACQUISINATOR_LOCKIN_STATUS_LOCKIN_SEQ_U "lockin_seq_u"
/* END */

/* START CANLIB UNKNOWN */
#define CANLIB_UNKNOWN "CANLIB_UNKNOWN"
/* END CANLIB UNKNOWN */

/* START TIMESTAMP */
#define TIMESTAMP "timestamp"
/* END TIMESTAMP */

/* START MUX MESSAGES ID */
/* END MUX MESSAGES ID */
enum acquisinator_types_id {

  e_acquisinator_uint16_t = -4,
  e_acquisinator_uint8_t,
  e_acquisinator_float,
  e_acquisinator_bool,

};

/**
 * @brief get the name of the signals in the message
 *
 * @param[in] id message id
 * @param[out] v array of strings containing the name of the signals
 * @param[in] fields_size maximum size of v
 * @param[in] string_size maximum size of v[i]
 *
 * @return 0 if ok 1 otherwise
 */
int acquisinator_fields_string_from_id(enum CanAcquisinatorMessageFrameId id,
                                       char **v, size_t fields_size,
                                       size_t string_size);

/**
 * @brief get the fields of an enum given the id of the enum (get the id from
 * fields_types_from_id)
 *
 * @param[in] enum_id the id of the enum, you can get it from
 * fields_types_from_id
 * @param[out] v array of strings containing the enum fields
 * @param[in] fields_size maximum size of v
 * @param[in] string_size maximum size of v[i]
 *
 * @return 0 if ok 1 otherwise
 */
int acquisinator_enum_fields(int enum_id, char **v, size_t fields_size,
                             size_t string_size);

/**
 * @brief serialize to a data pointer from a message id
 *
 * @param[in] id message id
 * @param[in] s string containing the data to serialize (comma separated)
 * @param[out] data pointer to the serialized data
 * @param[out] size size of the message
 *
 * @return Size of packed data, or negative error code.
 */
int acquisinator_serialize_from_string(enum CanAcquisinatorMessageFrameId id,
                                       char *s, uint8_t *data, size_t *size);

/**
 * @brief get the number of signals in the message
 *
 * @param[in] id the id of the message
 *
 * @return return the number of the signals
 */
int acquisinator_n_fields_from_id(enum CanAcquisinatorMessageFrameId id);

/**
 * @brief get the types of the signals in the message
 *
 * @param[in] id the id of the message
 * @param[out] fields_types fields_types[i] contains the type id of the signal i
 * (must be already allocated)
 * @param[in] fields_types_size max size of fields_types
 *
 * @return the number of types set, 0 if the id is invalid or fields_types_size
 * is too small
 */
int acquisinator_fields_types_from_id(enum CanAcquisinatorMessageFrameId id,
                                      int *fields_types, int fields_types_size);

/**
 * @brief get the fields of an enum given the name of the message and the name
 * of the signal
 *
 * @param[in] msg_name name of the message to find
 * @param[in] sgn_name name of the signal to find
 * @param[out] v array of strings containing the enum fields
 *
 * @return the number of fields or 0 if the signal is not an enum
 */
int acquisinator_enum_fields_from_name(const char *msg_name,
                                       const char *sgn_name, char **v);

/*!
 * \brief parse message fields to a string given a message
 * \param[in] the message to be parsed
 * \param[out] string buffer that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_acquisinator_raw_to_string(
    struct CanAcquisinatorAcquisinatorRaw message, char *buffer);

/*!
 * \brief parse message fields to a file descriptor given a message
 * \param[in] the message to be parsed
 * \param[out] file descriptor that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_acquisinator_raw_to_string_file(
    struct CanAcquisinatorAcquisinatorRaw message, FILE *buffer);

/*!
 * \brief parse message fields to a string given a message
 * \param[in] the message to be parsed
 * \param[out] string buffer that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_acquisinator_raw_fields(char *buffer);

/*!
 * \brief parse message fields to a file descriptor given a message
 * \param[in] the message to be parsed
 * \param[out] file descriptor that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_acquisinator_raw_fields_file(FILE *buffer);

/*!
 * \brief parse message fields to a string given a message
 * \param[in] the message to be parsed
 * \param[out] string buffer that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_acquisinator_lockin_to_string(
    struct CanAcquisinatorAcquisinatorLockin message, char *buffer);

/*!
 * \brief parse message fields to a file descriptor given a message
 * \param[in] the message to be parsed
 * \param[out] file descriptor that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_acquisinator_lockin_to_string_file(
    struct CanAcquisinatorAcquisinatorLockin message, FILE *buffer);

/*!
 * \brief parse message fields to a string given a message
 * \param[in] the message to be parsed
 * \param[out] string buffer that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_acquisinator_lockin_fields(char *buffer);

/*!
 * \brief parse message fields to a file descriptor given a message
 * \param[in] the message to be parsed
 * \param[out] file descriptor that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_acquisinator_lockin_fields_file(FILE *buffer);

/*!
 * \brief parse message fields to a string given a message
 * \param[in] the message to be parsed
 * \param[out] string buffer that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_acquisinator_lockin_status_to_string(
    struct CanAcquisinatorAcquisinatorLockinStatus message, char *buffer);

/*!
 * \brief parse message fields to a file descriptor given a message
 * \param[in] the message to be parsed
 * \param[out] file descriptor that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_acquisinator_lockin_status_to_string_file(
    struct CanAcquisinatorAcquisinatorLockinStatus message, FILE *buffer);

/*!
 * \brief parse message fields to a string given a message
 * \param[in] the message to be parsed
 * \param[out] string buffer that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_acquisinator_lockin_status_fields(char *buffer);

/*!
 * \brief parse message fields to a file descriptor given a message
 * \param[in] the message to be parsed
 * \param[out] file descriptor that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_acquisinator_lockin_status_fields_file(FILE *buffer);

/*!
 * \brief put signal names of messages in a string given a specific message id
 * \param[in] id of the message to be parsed
 * \param[in] generic pointer of message to be parsed
 * \param[out] string buffer that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_fields_from_id(enum CanAcquisinatorMessageFrameId message_id,
                                char *buffer);

/*!
 * \brief parse message fields to a string given a message id
 * \param[in] id of the message to be parsed
 * \param[in] generic pointer of message to be parsed
 * \param[out] file descriptor of the output file
 * \return the written number of bytes, negative value on error
 */
int acquisinator_to_string_file_from_id(
    enum CanAcquisinatorMessageFrameId message_id,
    union CanAcquisinatorMessage *messages, FILE *buffer);

/*!
 * \brief put signal names of messages in a file given a specific message id
 * \param[in] id of the message to be parsed
 * \param[in] generic pointer of message to be parsed
 * \param[out] file descriptor of the output file
 * \return the written number of bytes, negative value on error
 */
int acquisinator_fields_file_from_id(
    enum CanAcquisinatorMessageFrameId message_id, FILE *buffer);

/*!
 * \brief parse message fields to a string given a message id
 * \param[in] id of the message to be parsed
 * \param[in] generic pointer of message to be parsed
 * \param[out] string buffer that contains the string output
 * \return the written number of bytes, negative value on error
 */
int acquisinator_to_string_from_id(
    enum CanAcquisinatorMessageFrameId message_id,
    union CanAcquisinatorMessage *messages, char *buffer);

/**
 * @brief get the fields of an enum given the name of the message and the name
 * of the signal
 * @param[in] id: the id value to be checked
 * @return true if id is valid, false otherwise
 */
bool acquisinator_id_is_message(enum CanAcquisinatorMessageFrameId id);

int acquisinator_message_name_from_id(
    enum CanAcquisinatorMessageFrameId message_id, char *buffer);
#ifdef __cplusplus
}
#endif

#endif // acquisinator_UTILS_H