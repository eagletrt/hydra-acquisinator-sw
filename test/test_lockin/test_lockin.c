#include "unity.h"
#include "fff.h"

#include <math.h>

// Included to reach the module's static handler; libcan is the real one so the
// test also checks the bytes that end up on the bus.
#include "../../Core/Src/acquisinator/lockin-api.c"

DEFINE_FFF_GLOBALS;

FAKE_VALUE_FUNC(enum CanCommunicationReturnCode, can_communications_api_add_to_tx_buffer, const struct CanCommunicationFrame *);

#define DC 20000.0F
#define AMP 1000.0F

static struct CanCommunicationFrame sent[2];
static int sent_count;

static enum CanCommunicationReturnCode tx_ok(const struct CanCommunicationFrame *frame) {
    if (sent_count < 2) {
        sent[sent_count] = *frame;
    }
    ++sent_count;
    return CAN_COMMUNICATION_RC_OK;
}

// One sample per tick: dc + amp*sin(phase of the excitation point that produced it + phi)
static void feed(uint32_t count, float phi) {
    for (uint32_t n = 0U; n < count; ++n) {
        const float theta = 2.0F * (float)M_PI * (float)lockin_handler.index / (float)LOCKIN_SAMPLES_PER_PERIOD;
        (void)lockin_api_on_sample(true, (uint16_t)lroundf(DC + AMP * sinf(theta + phi)));
    }
}

void setUp(void) {
    RESET_FAKE(can_communications_api_add_to_tx_buffer);
    can_communications_api_add_to_tx_buffer_fake.custom_fake = tx_ok;
    sent_count = 0;
}

void tearDown(void) {}

void test_init_returns_excitation_dc(void) {
    // 0.55 V of 3.3 V full scale
    TEST_ASSERT_UINT16_WITHIN(1U, 683U, lockin_api_init());
}

void test_dac_table_spans_excitation_range(void) {
    (void)lockin_api_init();
    uint16_t lo = UINT16_MAX;
    uint16_t hi = 0U;
    for (uint32_t k = 0U; k < LOCKIN_SAMPLES_PER_PERIOD; ++k) {
        lo = lockin_handler.dac_table[k] < lo ? lockin_handler.dac_table[k] : lo;
        hi = lockin_handler.dac_table[k] > hi ? lockin_handler.dac_table[k] : hi;
    }
    TEST_ASSERT_UINT16_WITHIN(1U, 310U, lo);  // 0.25 V
    TEST_ASSERT_UINT16_WITHIN(1U, 1055U, hi); // 0.85 V
}

void test_dac_advances_every_sample_even_on_spi_error(void) {
    (void)lockin_api_init();
    TEST_ASSERT_EQUAL_UINT16(lockin_handler.dac_table[1], lockin_api_on_sample(false, 0U));
    TEST_ASSERT_EQUAL_UINT16(lockin_handler.dac_table[2], lockin_api_on_sample(true, 100U));
}

void test_nothing_sent_before_a_full_window(void) {
    (void)lockin_api_init();
    feed(LOCKIN_WINDOW_SAMPLES - 1U, 0.0F);
    TEST_ASSERT_EQUAL(LOCKIN_RC_OK, lockin_api_send_result());
    TEST_ASSERT_EQUAL_INT(0, sent_count);
}

void test_result_recovers_amplitude_and_phase(void) {
    const float phi = 0.6F;
    (void)lockin_api_init();
    feed(LOCKIN_WINDOW_SAMPLES, phi);

    TEST_ASSERT_EQUAL(LOCKIN_RC_OK, lockin_api_send_result());
    TEST_ASSERT_EQUAL_INT(2, sent_count);
    TEST_ASSERT_EQUAL_UINT32(CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN, sent[0].id);
    TEST_ASSERT_EQUAL_UINT32(CAN_ACQUISINATOR_MESSAGE_FRAME_ID_ACQUISINATOR_LOCKIN_STATUS, sent[1].id);

    union CanAcquisinatorMessage decoded = { 0 };
    TEST_ASSERT_TRUE(can_acquisinator_api_deserialize_from_id(sent[0].id, sent[0].data, &decoded) >= 0);
    TEST_ASSERT_FLOAT_WITHIN(1.0F, AMP * cosf(phi), decoded.acquisinator_lockin.lockin_inphase);
    TEST_ASSERT_FLOAT_WITHIN(1.0F, AMP * sinf(phi), decoded.acquisinator_lockin.lockin_quadrature);

    TEST_ASSERT_TRUE(can_acquisinator_api_deserialize_from_id(sent[1].id, sent[1].data, &decoded) >= 0);
    TEST_ASSERT_UINT16_WITHIN(1U, 20000U, decoded.acquisinator_lockin_status.lockin_dc_u);
    TEST_ASSERT_UINT16_WITHIN(2U, 19000U, decoded.acquisinator_lockin_status.lockin_min_u);
    TEST_ASSERT_UINT16_WITHIN(2U, 21000U, decoded.acquisinator_lockin_status.lockin_max_u);
    TEST_ASSERT_FALSE(decoded.acquisinator_lockin_status.lockin_spi_error_b);
    TEST_ASSERT_EQUAL_UINT8(0U, decoded.acquisinator_lockin_status.lockin_seq_u);
}

void test_dc_and_50hz_are_rejected(void) {
    (void)lockin_api_init();
    for (uint32_t n = 0U; n < LOCKIN_WINDOW_SAMPLES; ++n) {
        const float t = (float)n / (float)LOCKIN_SAMPLE_RATE_HZ;
        (void)lockin_api_on_sample(true, (uint16_t)lroundf(DC + 5000.0F * sinf(2.0F * (float)M_PI * 50.0F * t + 0.3F)));
    }
    TEST_ASSERT_TRUE(lockin_handler.ready);
    TEST_ASSERT_FLOAT_WITHIN(1.0F, 0.0F, lockin_handler.i);
    TEST_ASSERT_FLOAT_WITHIN(1.0F, 0.0F, lockin_handler.q);
}

void test_spi_error_restarts_window_and_is_flagged(void) {
    (void)lockin_api_init();
    feed(10U, 0.0F);
    (void)lockin_api_on_sample(false, 0U);
    feed(LOCKIN_WINDOW_SAMPLES - 1U, 0.0F);
    TEST_ASSERT_FALSE(lockin_handler.ready);
    feed(1U, 0.0F);
    TEST_ASSERT_TRUE(lockin_handler.ready);
    TEST_ASSERT_TRUE(lockin_handler.result_spi_error);
}

void test_unsent_result_is_dropped_and_leaves_a_sequence_gap(void) {
    (void)lockin_api_init();
    feed(2U * LOCKIN_WINDOW_SAMPLES, 0.0F); // second window dropped
    TEST_ASSERT_EQUAL(LOCKIN_RC_OK, lockin_api_send_result());
    feed(LOCKIN_WINDOW_SAMPLES, 0.0F);
    TEST_ASSERT_EQUAL_UINT8(2U, lockin_handler.result_sequence);
}

void test_queue_full_is_reported(void) {
    (void)lockin_api_init();
    feed(LOCKIN_WINDOW_SAMPLES, 0.0F);
    can_communications_api_add_to_tx_buffer_fake.custom_fake = NULL;
    can_communications_api_add_to_tx_buffer_fake.return_val = CAN_COMMUNICATION_RC_QUEUE_FULL;
    TEST_ASSERT_EQUAL(LOCKIN_RC_ERROR, lockin_api_send_result());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_returns_excitation_dc);
    RUN_TEST(test_dac_table_spans_excitation_range);
    RUN_TEST(test_dac_advances_every_sample_even_on_spi_error);
    RUN_TEST(test_nothing_sent_before_a_full_window);
    RUN_TEST(test_result_recovers_amplitude_and_phase);
    RUN_TEST(test_dc_and_50hz_are_rejected);
    RUN_TEST(test_spi_error_restarts_window_and_is_flagged);
    RUN_TEST(test_unsent_result_is_dropped_and_leaves_a_sequence_gap);
    RUN_TEST(test_queue_full_is_reported);
    return UNITY_END();
}
