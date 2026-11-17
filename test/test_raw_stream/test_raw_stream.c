#include "unity.h"
#include "fff.h"

// Included to reach the module's static handler; libcan is the real one so the
// test also checks the bytes that end up on the bus.
#include "../../Core/Src/acquisinator/raw-stream-api.c"

DEFINE_FFF_GLOBALS;

FAKE_VOID_FUNC(fake_cs_enter);
FAKE_VOID_FUNC(fake_cs_exit);
FAKE_VALUE_FUNC(enum CanCommunicationReturnCode, can_communications_api_add_to_tx_buffer, const struct CanCommunicationFrame *);

// The module passes a pointer to a stack frame: keep a copy of the last one
static struct CanCommunicationFrame sent;

static enum CanCommunicationReturnCode tx_ok(const struct CanCommunicationFrame *frame) {
    sent = *frame;
    return CAN_COMMUNICATION_RC_OK;
}

static void feed(uint16_t first, uint16_t count) {
    for (uint16_t i = 0U; i < count; ++i) {
        raw_stream_api_on_sample((uint16_t)(first + i));
    }
}

void setUp(void) {
    RESET_FAKE(fake_cs_enter);
    RESET_FAKE(fake_cs_exit);
    RESET_FAKE(can_communications_api_add_to_tx_buffer);
    FFF_RESET_HISTORY();
    can_communications_api_add_to_tx_buffer_fake.custom_fake = tx_ok;
    memset(&sent, 0, sizeof(sent));
    TEST_ASSERT_EQUAL(RAW_STREAM_RC_OK, raw_stream_api_init((struct RawStreamConfig){
                                            .cs_enter = fake_cs_enter,
                                            .cs_exit = fake_cs_exit,
                                        }));
}

void tearDown(void) {
}

void test_init_rejects_missing_callback(void) {
    TEST_ASSERT_EQUAL(RAW_STREAM_RC_NULL_POINTER, raw_stream_api_init((struct RawStreamConfig){ .cs_enter = fake_cs_enter }));
}

void test_nothing_sent_before_a_full_batch(void) {
    feed(100U, RAW_STREAM_SAMPLES_PER_FRAME - 1U);

    TEST_ASSERT_EQUAL(RAW_STREAM_RC_OK, raw_stream_api_process());
    TEST_ASSERT_EQUAL(0, can_communications_api_add_to_tx_buffer_fake.call_count);
}

// Frame layout must match the DBC: 3 x uint16 little endian (oldest first), then the sequence
void test_full_batch_becomes_one_frame(void) {
    raw_stream_api_on_sample(0x1234U);
    raw_stream_api_on_sample(0x5678U);
    raw_stream_api_on_sample(0x9ABCU);

    TEST_ASSERT_EQUAL(RAW_STREAM_RC_OK, raw_stream_api_process());
    TEST_ASSERT_EQUAL(1, can_communications_api_add_to_tx_buffer_fake.call_count);
    TEST_ASSERT_EQUAL_HEX32(0x12U, sent.id);
    TEST_ASSERT_EQUAL(8, sent.length);
    const uint8_t expected[7] = { 0x34U, 0x12U, 0x78U, 0x56U, 0xBCU, 0x9AU, 0x00U };
    TEST_ASSERT_EQUAL_HEX8_ARRAY(expected, sent.data, sizeof(expected));

    // Already sent: nothing more until the next batch
    TEST_ASSERT_EQUAL(RAW_STREAM_RC_OK, raw_stream_api_process());
    TEST_ASSERT_EQUAL(1, can_communications_api_add_to_tx_buffer_fake.call_count);
}

void test_sequence_advances_per_frame(void) {
    feed(0U, RAW_STREAM_SAMPLES_PER_FRAME);
    TEST_ASSERT_EQUAL(RAW_STREAM_RC_OK, raw_stream_api_process());
    feed(0U, RAW_STREAM_SAMPLES_PER_FRAME);
    TEST_ASSERT_EQUAL(RAW_STREAM_RC_OK, raw_stream_api_process());

    TEST_ASSERT_EQUAL(1, sent.data[6]);
}

// Main loop too slow: the older batch is overwritten, counted, and the sequence jumps
// so the receiver sees the gap
void test_overwritten_batch_is_dropped_and_visible(void) {
    feed(10U, RAW_STREAM_SAMPLES_PER_FRAME);
    feed(20U, RAW_STREAM_SAMPLES_PER_FRAME);

    TEST_ASSERT_EQUAL(1, raw_stream_api_get_dropped_count());
    TEST_ASSERT_EQUAL(RAW_STREAM_RC_OK, raw_stream_api_process());
    TEST_ASSERT_EQUAL(1, can_communications_api_add_to_tx_buffer_fake.call_count);
    TEST_ASSERT_EQUAL_HEX8(20U, sent.data[0]); // newest batch kept
    TEST_ASSERT_EQUAL(1, sent.data[6]);        // sequence 0 never reached the bus
}

void test_batch_is_read_inside_critical_section(void) {
    feed(0U, RAW_STREAM_SAMPLES_PER_FRAME);
    TEST_ASSERT_EQUAL(RAW_STREAM_RC_OK, raw_stream_api_process());

    TEST_ASSERT_EQUAL(1, fake_cs_enter_fake.call_count);
    TEST_ASSERT_EQUAL(1, fake_cs_exit_fake.call_count);
    TEST_ASSERT_EQUAL_PTR((void *)fake_cs_enter, fff.call_history[0]);
    TEST_ASSERT_EQUAL_PTR((void *)fake_cs_exit, fff.call_history[1]);
}

void test_full_tx_queue_is_reported(void) {
    can_communications_api_add_to_tx_buffer_fake.custom_fake = NULL;
    can_communications_api_add_to_tx_buffer_fake.return_val = CAN_COMMUNICATION_RC_QUEUE_FULL;
    feed(0U, RAW_STREAM_SAMPLES_PER_FRAME);

    TEST_ASSERT_EQUAL(RAW_STREAM_RC_QUEUE, raw_stream_api_process());
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_init_rejects_missing_callback);
    RUN_TEST(test_nothing_sent_before_a_full_batch);
    RUN_TEST(test_full_batch_becomes_one_frame);
    RUN_TEST(test_sequence_advances_per_frame);
    RUN_TEST(test_overwritten_batch_is_dropped_and_visible);
    RUN_TEST(test_batch_is_read_inside_critical_section);
    RUN_TEST(test_full_tx_queue_is_reported);
    return UNITY_END();
}
