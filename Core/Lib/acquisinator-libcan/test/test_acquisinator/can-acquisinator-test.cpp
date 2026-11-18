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
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MIN           \
  (-214748.3648f)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MIN        \
  (-214748.3648f)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_DC_U_MIN       \
  (CAN_ACQUISINATOR_TEST_UINT16_ZERO)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_MIN_U_MIN      \
  (CAN_ACQUISINATOR_TEST_UINT16_ZERO)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_MAX_U_MIN      \
  (CAN_ACQUISINATOR_TEST_UINT16_ZERO)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_SPI_ERROR_B_MIN \
  (CAN_ACQUISINATOR_TEST_BOOL_ZERO)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_SEQ_U_MIN      \
  (CAN_ACQUISINATOR_TEST_UINT8_ZERO)

#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW0_U_MAX            \
  (static_cast<uint16_t>(65535))
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW1_U_MAX            \
  (static_cast<uint16_t>(65535))
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_ADC_RAW2_U_MAX            \
  (static_cast<uint16_t>(65535))
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_RAW_DEBUG_SEQ_U_MAX                 \
  (static_cast<uint8_t>(255))
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MAX           \
  (214748.3647f)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MAX        \
  (214748.3647f)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_DC_U_MAX       \
  (static_cast<uint16_t>(65535))
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_MIN_U_MAX      \
  (static_cast<uint16_t>(65535))
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_MAX_U_MAX      \
  (static_cast<uint16_t>(65535))
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_SPI_ERROR_B_MAX \
  (CAN_ACQUISINATOR_TEST_BOOL_ZERO)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_SEQ_U_MAX      \
  (static_cast<uint8_t>(255))

#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_TOLERANCE     \
  (0.000101f)
#define CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_TOLERANCE  \
  (0.000101f)

TEST(CanAcquisinatorTestIdIsValidTest, AcquisinatorRawIdTest) {
  EXPECT_TRUE(can_acquisinator_api_id_is_valid(
      static_cast<enum CanAcquisinatorMessageFrameId>(
          CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_RAW)));
}
TEST(CanAcquisinatorTestIdIsValidTest, AcquisinatorLockinIdTest) {
  EXPECT_TRUE(can_acquisinator_api_id_is_valid(
      static_cast<enum CanAcquisinatorMessageFrameId>(
          CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN)));
}
TEST(CanAcquisinatorTestIdIsValidTest, AcquisinatorLockinStatusIdTest) {
  EXPECT_TRUE(can_acquisinator_api_id_is_valid(
      static_cast<enum CanAcquisinatorMessageFrameId>(
          CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS)));
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
TEST(CanAcquisinatorTestIndexIsValidTest, AcquisinatorLockinIndexTest) {
  EXPECT_TRUE(can_acquisinator_api_index_is_valid(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN));
}
TEST(CanAcquisinatorTestIndexIsValidTest, AcquisinatorLockinStatusIndexTest) {
  EXPECT_TRUE(can_acquisinator_api_index_is_valid(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS));
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
TEST(CanAcquisinatorTestIndexFromIdTest, AcquisinatorLockinIdTest) {
  EXPECT_EQ(can_acquisinator_api_index_from_id(
                static_cast<enum CanAcquisinatorMessageFrameId>(
                    CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN)),
            CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN);
}
TEST(CanAcquisinatorTestIndexFromIdTest, AcquisinatorLockinStatusIdTest) {
  EXPECT_EQ(
      can_acquisinator_api_index_from_id(
          static_cast<enum CanAcquisinatorMessageFrameId>(
              CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS)),
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS);
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
TEST(CanAcquisinatorTestIdFromIndexTest, AcquisinatorLockinIndexTest) {
  EXPECT_EQ(can_acquisinator_api_id_from_index(
                CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN),
            CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN);
}
TEST(CanAcquisinatorTestIdFromIndexTest, AcquisinatorLockinStatusIndexTest) {
  EXPECT_EQ(can_acquisinator_api_id_from_index(
                CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS),
            CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS);
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

TEST(CanAcquisinatorTestEncodeDecodeTest,
     AcquisinatorLockinLockinInphaseEncodeDecodeMinimumTest) {
  EXPECT_NEAR(
      prv_acquisinator_lockin_lockin_inphase_decode(
          prv_acquisinator_lockin_lockin_inphase_encode(
              CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MIN)),
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MIN,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_TOLERANCE);
}
TEST(CanAcquisinatorTestEncodeDecodeTest,
     AcquisinatorLockinLockinInphaseEncodeDecodeMaximumTest) {
  EXPECT_NEAR(
      prv_acquisinator_lockin_lockin_inphase_decode(
          prv_acquisinator_lockin_lockin_inphase_encode(
              CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MAX)),
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MAX,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_TOLERANCE);
}
TEST(CanAcquisinatorTestEncodeDecodeTest,
     AcquisinatorLockinLockinInphaseEncodeDecodeUnderflowTest) {
  EXPECT_NEAR(
      prv_acquisinator_lockin_lockin_inphase_decode(
          prv_acquisinator_lockin_lockin_inphase_encode(
              CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MIN -
              0.0001f)),
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MIN,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_TOLERANCE);
}
TEST(CanAcquisinatorTestEncodeDecodeTest,
     AcquisinatorLockinLockinInphaseEncodeDecodeOverflowTest) {
  EXPECT_NEAR(
      prv_acquisinator_lockin_lockin_inphase_decode(
          prv_acquisinator_lockin_lockin_inphase_encode(
              CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MAX +
              0.0001f)),
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MAX,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_TOLERANCE);
}
TEST(CanAcquisinatorTestEncodeDecodeTest,
     AcquisinatorLockinLockinInphaseEncodeDecodeMiddleValueTest) {
  EXPECT_NEAR(
      prv_acquisinator_lockin_lockin_inphase_decode(
          prv_acquisinator_lockin_lockin_inphase_encode(
              -5.0000002374872565e-05f)),
      -5.0000002374872565e-05f,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_TOLERANCE);
}
TEST(CanAcquisinatorTestEncodeDecodeTest,
     AcquisinatorLockinLockinQuadratureEncodeDecodeMinimumTest) {
  EXPECT_NEAR(
      prv_acquisinator_lockin_lockin_quadrature_decode(
          prv_acquisinator_lockin_lockin_quadrature_encode(
              CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MIN)),
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MIN,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_TOLERANCE);
}
TEST(CanAcquisinatorTestEncodeDecodeTest,
     AcquisinatorLockinLockinQuadratureEncodeDecodeMaximumTest) {
  EXPECT_NEAR(
      prv_acquisinator_lockin_lockin_quadrature_decode(
          prv_acquisinator_lockin_lockin_quadrature_encode(
              CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MAX)),
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MAX,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_TOLERANCE);
}
TEST(CanAcquisinatorTestEncodeDecodeTest,
     AcquisinatorLockinLockinQuadratureEncodeDecodeUnderflowTest) {
  EXPECT_NEAR(
      prv_acquisinator_lockin_lockin_quadrature_decode(
          prv_acquisinator_lockin_lockin_quadrature_encode(
              CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MIN -
              0.0001f)),
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MIN,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_TOLERANCE);
}
TEST(CanAcquisinatorTestEncodeDecodeTest,
     AcquisinatorLockinLockinQuadratureEncodeDecodeOverflowTest) {
  EXPECT_NEAR(
      prv_acquisinator_lockin_lockin_quadrature_decode(
          prv_acquisinator_lockin_lockin_quadrature_encode(
              CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MAX +
              0.0001f)),
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MAX,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_TOLERANCE);
}
TEST(CanAcquisinatorTestEncodeDecodeTest,
     AcquisinatorLockinLockinQuadratureEncodeDecodeMiddleValueTest) {
  EXPECT_NEAR(
      prv_acquisinator_lockin_lockin_quadrature_decode(
          prv_acquisinator_lockin_lockin_quadrature_encode(
              -5.0000002374872565e-05f)),
      -5.0000002374872565e-05f,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_TOLERANCE);
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
TEST(CanAcquisinatorTestSerializeDeserializeTest, AcquisinatorLockinMinimum) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;
  u1->acquisinator_lockin.lockin_inphase =
      (float)CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MIN;
  u1->acquisinator_lockin.lockin_quadrature =
      (float)CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MIN;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN, dst_p, u2);
  EXPECT_NEAR(
      u1->acquisinator_lockin.lockin_inphase,
      u2->acquisinator_lockin.lockin_inphase,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_TOLERANCE);
  EXPECT_NEAR(
      u1->acquisinator_lockin.lockin_quadrature,
      u2->acquisinator_lockin.lockin_quadrature,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_TOLERANCE);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest, AcquisinatorLockinMaximum) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;
  u1->acquisinator_lockin.lockin_inphase =
      (float)CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MAX;
  u1->acquisinator_lockin.lockin_quadrature =
      (float)CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MAX;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN, dst_p, u2);
  EXPECT_NEAR(
      u1->acquisinator_lockin.lockin_inphase,
      u2->acquisinator_lockin.lockin_inphase,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_TOLERANCE);
  EXPECT_NEAR(
      u1->acquisinator_lockin.lockin_quadrature,
      u2->acquisinator_lockin.lockin_quadrature,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_TOLERANCE);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest,
     AcquisinatorLockinUnderflowTest) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;
  u1->acquisinator_lockin.lockin_inphase =
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MIN - 0.0001f;
  u1->acquisinator_lockin.lockin_quadrature =
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MIN - 0.0001f;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN, dst_p, u2);
  EXPECT_NEAR(
      u2->acquisinator_lockin.lockin_inphase,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MIN,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_TOLERANCE);
  EXPECT_NEAR(
      u2->acquisinator_lockin.lockin_quadrature,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MIN,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_TOLERANCE);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest,
     AcquisinatorLockinOverflowTest) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;
  u1->acquisinator_lockin.lockin_inphase =
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MAX + 0.0001f;
  u1->acquisinator_lockin.lockin_quadrature =
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MAX + 0.0001f;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN, dst_p, u2);
  EXPECT_NEAR(
      u2->acquisinator_lockin.lockin_inphase,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_MAX,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_TOLERANCE);
  EXPECT_NEAR(
      u2->acquisinator_lockin.lockin_quadrature,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_MAX,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_TOLERANCE);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest,
     AcquisinatorLockinMiddleValueTest) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;
  u1->acquisinator_lockin.lockin_inphase = -5.0000002374872565e-05f;
  u1->acquisinator_lockin.lockin_quadrature = -5.0000002374872565e-05f;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN, dst_p, u2);
  EXPECT_NEAR(
      u1->acquisinator_lockin.lockin_inphase,
      u2->acquisinator_lockin.lockin_inphase,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_INPHASE_TOLERANCE);
  EXPECT_NEAR(
      u1->acquisinator_lockin.lockin_quadrature,
      u2->acquisinator_lockin.lockin_quadrature,
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_LOCKIN_QUADRATURE_TOLERANCE);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest,
     AcquisinatorLockinStatusMinimum) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;
  u1->acquisinator_lockin_status.lockin_dc_u = (uint16_t)
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_DC_U_MIN;
  u1->acquisinator_lockin_status.lockin_min_u = (uint16_t)
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_MIN_U_MIN;
  u1->acquisinator_lockin_status.lockin_max_u = (uint16_t)
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_MAX_U_MIN;
  u1->acquisinator_lockin_status.lockin_spi_error_b = (bool)
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_SPI_ERROR_B_MIN;
  u1->acquisinator_lockin_status.lockin_seq_u = (uint8_t)
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_SEQ_U_MIN;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS, dst_p, u2);
  EXPECT_EQ(u1->acquisinator_lockin_status.lockin_dc_u,
            u2->acquisinator_lockin_status.lockin_dc_u);
  EXPECT_EQ(u1->acquisinator_lockin_status.lockin_min_u,
            u2->acquisinator_lockin_status.lockin_min_u);
  EXPECT_EQ(u1->acquisinator_lockin_status.lockin_max_u,
            u2->acquisinator_lockin_status.lockin_max_u);
  EXPECT_EQ(u1->acquisinator_lockin_status.lockin_spi_error_b,
            u2->acquisinator_lockin_status.lockin_spi_error_b);
  EXPECT_EQ(u1->acquisinator_lockin_status.lockin_seq_u,
            u2->acquisinator_lockin_status.lockin_seq_u);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest,
     AcquisinatorLockinStatusMaximum) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;
  u1->acquisinator_lockin_status.lockin_dc_u = (uint16_t)
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_DC_U_MAX;
  u1->acquisinator_lockin_status.lockin_min_u = (uint16_t)
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_MIN_U_MAX;
  u1->acquisinator_lockin_status.lockin_max_u = (uint16_t)
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_MAX_U_MAX;
  u1->acquisinator_lockin_status.lockin_spi_error_b = (bool)
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_SPI_ERROR_B_MAX;
  u1->acquisinator_lockin_status.lockin_seq_u = (uint8_t)
      CAN_ACQUISINATOR_TEST_ACQUISINATOR_LOCKIN_STATUS_LOCKIN_SEQ_U_MAX;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS, dst_p, u2);
  EXPECT_EQ(u1->acquisinator_lockin_status.lockin_dc_u,
            u2->acquisinator_lockin_status.lockin_dc_u);
  EXPECT_EQ(u1->acquisinator_lockin_status.lockin_min_u,
            u2->acquisinator_lockin_status.lockin_min_u);
  EXPECT_EQ(u1->acquisinator_lockin_status.lockin_max_u,
            u2->acquisinator_lockin_status.lockin_max_u);
  EXPECT_EQ(u1->acquisinator_lockin_status.lockin_spi_error_b,
            u2->acquisinator_lockin_status.lockin_spi_error_b);
  EXPECT_EQ(u1->acquisinator_lockin_status.lockin_seq_u,
            u2->acquisinator_lockin_status.lockin_seq_u);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest,
     AcquisinatorLockinStatusUnderflowTest) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS, dst_p, u2);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest,
     AcquisinatorLockinStatusOverflowTest) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS, dst_p, u2);

  delete u2;
  delete[] dst_p;
  delete u1;
}
TEST(CanAcquisinatorTestSerializeDeserializeTest,
     AcquisinatorLockinStatusMiddleValueTest) {
  CanAcquisinatorMessage *u1 = new CanAcquisinatorMessage;

  uint8_t *dst_p = new uint8_t[8];
  (void)can_acquisinator_api_serialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS, u1, dst_p);

  CanAcquisinatorMessage *u2 = new CanAcquisinatorMessage;
  (void)can_acquisinator_api_deserialize_from_index(
      CAN_ACQUISINATOR_MESSAGE_INDEX_ACQUISINATOR_LOCKIN_STATUS, dst_p, u2);

  delete u2;
  delete[] dst_p;
  delete u1;
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();

  return 0;
}
