#include "acquisinator/can-acquisinator-api.c"
#include "acquisinator/can-acquisinator-api.h"
#include <gtest/gtest.h>

#define CAN_ACQUISINATOR_TEST_MINUS_ONE (-1)
#define CAN_ACQUISINATOR_TEST_ERROR (CAN_ACQUISINATOR_TEST_MINUS_ONE)
#define CAN_ACQUISINATOR_TEST_INVALID (CAN_ACQUISINATOR_TEST_MINUS_ONE)
#define CAN_ACQUISINATOR_TEST_UNDER_RANGE (CAN_ACQUISINATOR_TEST_MINUS_ONE)

#define CAN_ACQUISINATOR_TEST_FLOAT_ZERO (0.f)
#define CAN_ACQUISINATOR_TEST_INT8_ZERO (static_cast<int8_t>(0))
#define CAN_ACQUISINATOR_TEST_UINT8_ZERO (static_cast<uint8_t>(0))
#define CAN_ACQUISINATOR_TEST_INT16_ZERO (static_cast<int16_t>(0))
#define CAN_ACQUISINATOR_TEST_UINT16_ZERO (static_cast<uint16_t>(0))
#define CAN_ACQUISINATOR_TEST_INT32_ZERO (static_cast<int32_t>(0))
#define CAN_ACQUISINATOR_TEST_UINT32_ZERO (static_cast<uint32_t>(0))
#define CAN_ACQUISINATOR_TEST_INT64_ZERO (static_cast<int64_t>(0))
#define CAN_ACQUISINATOR_TEST_UINT64_ZERO (static_cast<uint64_t>(0))
#define CAN_ACQUISINATOR_TEST_BOOL_ZERO (static_cast<bool>(0))

#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW0_U_MIN            \
  (CAN_ACQUISINATOR_TEST_UINT16_ZERO)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW1_U_MIN            \
  (CAN_ACQUISINATOR_TEST_UINT16_ZERO)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW2_U_MIN            \
  (CAN_ACQUISINATOR_TEST_UINT16_ZERO)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_SEQ_U_MIN                 \
  (CAN_ACQUISINATOR_TEST_UINT8_ZERO)

#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW0_U_MAX            \
  (static_cast<uint16_t>(65535))
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW1_U_MAX            \
  (static_cast<uint16_t>(65535))
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW2_U_MAX            \
  (static_cast<uint16_t>(65535))
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_SEQ_U_MAX                 \
  (static_cast<uint8_t>(255))

TEST(CanAcquisinatorTestIdIsValidTest, AcquisinatorRawIdTest) {
  EXPECT_TRUE(can_acquisinator_api_id_is_valid(
      static_cast<enum CanAcquisinatorMessageFrameId>(
          CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW)));
}
TEST(CanAcquisinatorTestIdIsValidTest, CanAcquisinatorTestInvalidIdTest) {
  EXPECT_FALSE(can_acquisinator_api_id_is_valid(
      static_cast<enum CanAcquisinatorMessageFrameId>(
          CAN_ACQUISINATOR_TEST_INVALID)));
}

TEST(CanAcquisinatorTestIndexIsValidTest, AcquisinatorRawIndexTest) {
  EXPECT_TRUE(can_acquisinator_api_index_is_valid(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW));
}
TEST(CanAcquisinatorTestIndexIsValidTest, CanAcquisinatorTestUnderRangeTest) {
  EXPECT_FALSE(can_acquisinator_api_index_is_valid(
      static_cast<enum CanAcquisinatorMessageIndex>(
          CAN_ACQUISINATOR_TEST_UNDER_RANGE)));
}
TEST(CanAcquisinatorTestIndexIsValidTest, CanAcquisinatorTestOverRangeTest) {
  EXPECT_FALSE(
      can_acquisinator_api_index_is_valid(CAN_ACQUISINATOR_MESSAGE_COUNT));
}

TEST(CanAcquisinatorTestIndexFromIdTest, AcquisinatorRawIdTest) {
  EXPECT_EQ(can_acquisinator_api_index_from_id(
                static_cast<enum CanAcquisinatorMessageFrameId>(
                    CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW)),
            CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW);
}
TEST(CanAcquisinatorTestIndexFromIdTest, CanAcquisinatorTestInvalidIdTest) {
  EXPECT_EQ(can_acquisinator_api_index_from_id(
                static_cast<enum CanAcquisinatorMessageFrameId>(
                    CAN_ACQUISINATOR_TEST_INVALID)),
            CAN_ACQUISINATOR_MESSAGE_INDEX_INVALID);
}

TEST(CanAcquisinatorTestIdFromIndexTest, AcquisinatorRawIndexTest) {
  EXPECT_EQ(can_acquisinator_api_id_from_index(
                CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW),
            CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW);
}
TEST(CanAcquisinatorTestIdFromIndexTest, CanAcquisinatorTestUnderRangeTest) {
  EXPECT_EQ(can_acquisinator_api_id_from_index(
                static_cast<enum CanAcquisinatorMessageIndex>(
                    CAN_ACQUISINATOR_TEST_UNDER_RANGE)),
            CAN_ACQUISINATOR_MESSAGE_FRAME_ID_INVALID);
}
TEST(CanAcquisinatorTestIdFromIndexTest, CanAcquisinatorTestOverRangeTest) {
  EXPECT_EQ(can_acquisinator_api_id_from_index(CAN_ACQUISINATOR_MESSAGE_COUNT),
            CAN_ACQUISINATOR_MESSAGE_FRAME_ID_INVALID);
}

TEST(CanAcquisinatorTestSerializeDeserializeTest, AcquisinatorRawMinimum) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;
  u1->acquisinator_raw.debug_adc_raw0_u =
      (uint16_t)CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW0_U_MIN;
  u1->acquisinator_raw.debug_adc_raw1_u =
      (uint16_t)CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW1_U_MIN;
  u1->acquisinator_raw.debug_adc_raw2_u =
      (uint16_t)CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW2_U_MIN;
  u1->acquisinator_raw.debug_seq_u =
      (uint8_t)CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_SEQ_U_MIN;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW, dst_p, u2);
  EXPECT_EQ(u1->acquisinator_raw.debug_adc_raw0_u,
            u2->acquisinator_raw.debug_adc_raw0_u);
  EXPECT_EQ(u1->acquisinator_raw.debug_adc_raw1_u,
            u2->acquisinator_raw.debug_adc_raw1_u);
  EXPECT_EQ(u1->acquisinator_raw.debug_adc_raw2_u,
            u2->acquisinator_raw.debug_adc_raw2_u);
  EXPECT_EQ(u1->acquisinator_raw.debug_seq_u, u2->acquisinator_raw.debug_seq_u);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest, AcquisinatorRawMaximum) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;
  u1->acquisinator_raw.debug_adc_raw0_u =
      (uint16_t)CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW0_U_MAX;
  u1->acquisinator_raw.debug_adc_raw1_u =
      (uint16_t)CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW1_U_MAX;
  u1->acquisinator_raw.debug_adc_raw2_u =
      (uint16_t)CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW2_U_MAX;
  u1->acquisinator_raw.debug_seq_u =
      (uint8_t)CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_SEQ_U_MAX;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW, dst_p, u2);
  EXPECT_EQ(u1->acquisinator_raw.debug_adc_raw0_u,
            u2->acquisinator_raw.debug_adc_raw0_u);
  EXPECT_EQ(u1->acquisinator_raw.debug_adc_raw1_u,
            u2->acquisinator_raw.debug_adc_raw1_u);
  EXPECT_EQ(u1->acquisinator_raw.debug_adc_raw2_u,
            u2->acquisinator_raw.debug_adc_raw2_u);
  EXPECT_EQ(u1->acquisinator_raw.debug_seq_u, u2->acquisinator_raw.debug_seq_u);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest,
     AcquisinatorRawUnderflowTest) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW, dst_p, u2);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest, AcquisinatorRawOverflowTest) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW, dst_p, u2);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest,
     AcquisinatorRawMiddleValueTest) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_RAW, dst_p, u2);

  delete u2;
  delete[] dst_p;
  delete u1;
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();

  return 0;
}
