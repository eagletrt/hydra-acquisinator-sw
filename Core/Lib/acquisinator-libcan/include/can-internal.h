#ifndef CAN_INTERNAL_H
#define CAN_INTERNAL_H

/*!
 * \file can-internal.h
 * \brief CAN bus utility library with bit manipulation and data conversion
 * functions
 *
 * This header provides utility functions for packing and unpacking
 * CAN bus data with various bit shift operations for different integer types.
 */

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "eagletrt-api.h"

#ifndef CAN_PACKING
#if defined(__MINGW32__)
#define CAN_PACKING [[gnu::gcc_struct, gnu::packed]]
#else
#define CAN_PACKING [[gnu::packed]]
#endif
#endif

/*! One-shot mode constant */
constexpr int32_t can_oneshot = -1;

/*! \defgroup CAN_Utility_Functions CAN Bus Utility Functions
 *  \brief Helper functions for bit manipulation and data conversion.
 *  @{
 */

/*! \brief Pack value with left shift for uint8_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE
    uint8_t int_pack_left_shift_u8(uint8_t value, uint8_t shift, uint8_t mask) {
  return (uint8_t)((uint8_t)(value << shift) & mask);
}

/*! \brief Pack value with right shift for uint8_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint8_t int_pack_right_shift_u8(uint8_t value,
                                                       uint8_t shift,
                                                       uint8_t mask) {
  return (uint8_t)((uint8_t)(value >> shift) & mask);
}

/*! \brief Unpack value with left shift for uint8_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint8_t int_unpack_left_shift_u8(uint8_t value,
                                                        uint8_t shift,
                                                        uint8_t mask) {
  return (uint8_t)((uint8_t)(value & mask) << shift);
}

/*! \brief Unpack value with right shift for uint8_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint8_t int_unpack_right_shift_u8(uint8_t value,
                                                         uint8_t shift,
                                                         uint8_t mask) {
  return (uint8_t)((uint8_t)(value & mask) >> shift);
}

/*! \brief Pack value with left shift for uint16_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint16_t int_pack_left_shift_u16(uint16_t value,
                                                        uint8_t shift,
                                                        uint8_t mask) {
  return (uint16_t)((uint16_t)(value << shift) & mask);
}

/*! \brief Pack value with right shift for uint16_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint16_t int_pack_right_shift_u16(uint16_t value,
                                                         uint8_t shift,
                                                         uint8_t mask) {
  return (uint16_t)((uint16_t)(value >> shift) & mask);
}

/*! \brief Unpack value with left shift for uint16_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint16_t int_unpack_left_shift_u16(uint16_t value,
                                                          uint8_t shift,
                                                          uint8_t mask) {
  return (uint16_t)((uint16_t)(value & mask) << shift);
}

/*! \brief Unpack value with right shift for uint16_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint16_t int_unpack_right_shift_u16(uint16_t value,
                                                           uint8_t shift,
                                                           uint8_t mask) {
  return (uint16_t)((uint16_t)(value & mask) >> shift);
}

/*! \brief Pack value with left shift for uint32_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint32_t int_pack_left_shift_u32(uint32_t value,
                                                        uint8_t shift,
                                                        uint8_t mask) {
  return (uint32_t)((uint32_t)(value << shift) & mask);
}

/*! \brief Pack value with right shift for uint32_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint32_t int_pack_right_shift_u32(uint32_t value,
                                                         uint8_t shift,
                                                         uint8_t mask) {
  return (uint32_t)((uint32_t)(value >> shift) & mask);
}

/*! \brief Unpack value with left shift for uint32_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint32_t int_unpack_left_shift_u32(uint32_t value,
                                                          uint8_t shift,
                                                          uint8_t mask) {
  return (uint32_t)((uint32_t)(value & mask) << shift);
}

/*! \brief Unpack value with right shift for uint32_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint32_t int_unpack_right_shift_u32(uint32_t value,
                                                           uint8_t shift,
                                                           uint8_t mask) {
  return (uint32_t)((uint32_t)(value & mask) >> shift);
}

/*! \brief Pack value with left shift for uint64_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint64_t int_pack_left_shift_u64(uint64_t value,
                                                        uint8_t shift,
                                                        uint8_t mask) {
  return (uint64_t)((uint64_t)(value << shift) & mask);
}

/*! \brief Pack value with right shift for uint64_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint64_t int_pack_right_shift_u64(uint64_t value,
                                                         uint8_t shift,
                                                         uint8_t mask) {
  return (uint64_t)((uint64_t)(value >> shift) & mask);
}

/*! \brief Unpack value with left shift for uint64_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint64_t int_unpack_left_shift_u64(uint64_t value,
                                                          uint8_t shift,
                                                          uint8_t mask) {
  return (uint64_t)((uint64_t)(value & mask) << shift);
}

/*! \brief Unpack value with right shift for uint64_t */
[[nodiscard]]
EAGLETRT_STATIC_INLINE uint64_t int_unpack_right_shift_u64(uint64_t value,
                                                           uint8_t shift,
                                                           uint8_t mask) {
  return (uint64_t)((uint64_t)(value & mask) >> shift);
}
/*! @} */

#endif // CAN_INTERNAL_H