#include "unity.h"
#include "fff.h"

// Included to reach the module's static handler
#include "../../Core/Src/acquisinator/ltc1865l-api.c"

DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(fake_conv, bool);
FAKE_VOID_FUNC(fake_delay, uint32_t);
FAKE_VALUE_FUNC(enum Ltc1865lReturnCode, fake_spi, const uint8_t *, uint8_t *, size_t);

// SPI fake: records the TX bytes and answers with the configured RX bytes
static uint8_t spi_tx[ltc1865l_frame_size];
static uint8_t spi_rx[ltc1865l_frame_size];

static enum Ltc1865lReturnCode spi_ok(const uint8_t *tx, uint8_t *rx, size_t length) {
    memcpy(spi_tx, tx, length);
    memcpy(rx, spi_rx, length);
    return LTC1865L_RC_OK;
}

static struct Ltc1865lConfig config_for(enum Ltc1865lChannel channel) {
    return (struct Ltc1865lConfig){
        .spi_callback = fake_spi,
        .delay_callback = fake_delay,
        .conv_callback = fake_conv,
        .channel = channel,
    };
}

void setUp(void) {
    RESET_FAKE(fake_conv);
    RESET_FAKE(fake_delay);
    RESET_FAKE(fake_spi);
    FFF_RESET_HISTORY();
    fake_spi_fake.custom_fake = spi_ok;
    memset(spi_tx, 0, sizeof(spi_tx));
    memset(spi_rx, 0, sizeof(spi_rx));
    memset(&handler, 0, sizeof(handler));
}

void tearDown(void) {
}

void test_init_rejects_missing_callback(void) {
    struct Ltc1865lConfig config = config_for(LTC1865L_CHANNEL_0);
    config.delay_callback = NULL;

    TEST_ASSERT_EQUAL(LTC1865L_RC_NULL_POINTER, ltc1865l_api_init(config));
    TEST_ASSERT_EQUAL(0, fake_spi_fake.call_count);
}

void test_init_rejects_invalid_channel(void) {
    TEST_ASSERT_EQUAL(LTC1865L_RC_INVALID_CHANNEL, ltc1865l_api_init(config_for(LTC1865L_CHANNEL_COUNT)));
}

void test_read_before_init_fails(void) {
    uint16_t raw = 0U;
    TEST_ASSERT_EQUAL(LTC1865L_RC_NOT_INITIALIZED, ltc1865l_api_read(&raw));
    TEST_ASSERT_EQUAL(0, fake_spi_fake.call_count);
}

// The pipeline must be primed: init runs exactly one conversion selecting the channel
void test_init_primes_pipeline_with_one_conversion(void) {
    TEST_ASSERT_EQUAL(LTC1865L_RC_OK, ltc1865l_api_init(config_for(LTC1865L_CHANNEL_0)));
    TEST_ASSERT_EQUAL(1, fake_spi_fake.call_count);
}

// CONV high, wait tCONV, CONV low, then the SPI frame: any other order reads garbage
void test_conversion_sequence(void) {
    TEST_ASSERT_EQUAL(LTC1865L_RC_OK, ltc1865l_api_init(config_for(LTC1865L_CHANNEL_0)));

    TEST_ASSERT_EQUAL(4, fff.call_history_idx);
    TEST_ASSERT_EQUAL_PTR((void *)fake_conv, fff.call_history[0]);
    TEST_ASSERT_EQUAL_PTR((void *)fake_delay, fff.call_history[1]);
    TEST_ASSERT_EQUAL_PTR((void *)fake_conv, fff.call_history[2]);
    TEST_ASSERT_EQUAL_PTR((void *)fake_spi, fff.call_history[3]);

    TEST_ASSERT_TRUE(fake_conv_fake.arg0_history[0]);
    TEST_ASSERT_FALSE(fake_conv_fake.arg0_history[1]);
    TEST_ASSERT_GREATER_OR_EQUAL(5U, fake_delay_fake.arg0_val); // datasheet tCONV max 4.66 us
    TEST_ASSERT_EQUAL(ltc1865l_frame_size, fake_spi_fake.arg2_val);
}

// Single-ended select word: SGL/DIFF = 1, ODD/SIGN = channel (datasheet table 1)
void test_select_word_channel_0(void) {
    TEST_ASSERT_EQUAL(LTC1865L_RC_OK, ltc1865l_api_init(config_for(LTC1865L_CHANNEL_0)));
    TEST_ASSERT_EQUAL_HEX8(0x80U, spi_tx[0] & 0xC0U);
}

void test_select_word_channel_1(void) {
    TEST_ASSERT_EQUAL(LTC1865L_RC_OK, ltc1865l_api_init(config_for(LTC1865L_CHANNEL_1)));
    TEST_ASSERT_EQUAL_HEX8(0xC0U, spi_tx[0] & 0xC0U);
}

void test_read_returns_msb_first_result(void) {
    TEST_ASSERT_EQUAL(LTC1865L_RC_OK, ltc1865l_api_init(config_for(LTC1865L_CHANNEL_0)));
    spi_rx[0] = 0xBEU;
    spi_rx[1] = 0xEFU;

    uint16_t raw = 0U;
    TEST_ASSERT_EQUAL(LTC1865L_RC_OK, ltc1865l_api_read(&raw));
    TEST_ASSERT_EQUAL_HEX16(0xBEEFU, raw);
}

void test_read_spi_error_counts_and_keeps_output(void) {
    TEST_ASSERT_EQUAL(LTC1865L_RC_OK, ltc1865l_api_init(config_for(LTC1865L_CHANNEL_0)));
    fake_spi_fake.custom_fake = NULL;
    fake_spi_fake.return_val = LTC1865L_RC_SPI_ERROR;

    uint16_t raw = 0x1234U;
    TEST_ASSERT_EQUAL(LTC1865L_RC_SPI_ERROR, ltc1865l_api_read(&raw));
    TEST_ASSERT_EQUAL_HEX16(0x1234U, raw);
    TEST_ASSERT_EQUAL(1, ltc1865l_api_get_error_count());
}

void test_init_fails_if_priming_fails(void) {
    fake_spi_fake.custom_fake = NULL;
    fake_spi_fake.return_val = LTC1865L_RC_SPI_ERROR;

    TEST_ASSERT_EQUAL(LTC1865L_RC_SPI_ERROR, ltc1865l_api_init(config_for(LTC1865L_CHANNEL_0)));

    uint16_t raw = 0U;
    TEST_ASSERT_EQUAL(LTC1865L_RC_NOT_INITIALIZED, ltc1865l_api_read(&raw));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_rejects_missing_callback);
    RUN_TEST(test_init_rejects_invalid_channel);
    RUN_TEST(test_read_before_init_fails);
    RUN_TEST(test_init_primes_pipeline_with_one_conversion);
    RUN_TEST(test_conversion_sequence);
    RUN_TEST(test_select_word_channel_0);
    RUN_TEST(test_select_word_channel_1);
    RUN_TEST(test_read_returns_msb_first_result);
    RUN_TEST(test_read_spi_error_counts_and_keeps_output);
    RUN_TEST(test_init_fails_if_priming_fails);
    return UNITY_END();
}
