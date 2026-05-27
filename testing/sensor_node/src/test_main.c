#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "coms.h"
#include "sensor_node.h"

extern sensor_node_t sn;
extern struct k_msgq packet_queue;

extern struct k_msgq packet_queue;
extern struct k_sem emergency_sem;
extern int dynamic_set_variable;

void sensor_emergency_isr(void);
void process_packet(packet_t *packet);


#define ADXL_NODE DT_NODELABEL(adxl_313)
#define FSR_NODE  DT_NODELABEL(fsr_sensor)
#define TOF_NODE  DT_NODELABEL(vl53l1x0)

ZTEST(sensor_node_suite, test_build_request_packet)
{
    packet_t *pkt = build_packet(REQUEST, NULL, 0);

    zassert_not_null(pkt, "packet should not be NULL");
    zassert_equal(pkt->type, REQUEST, "wrong packet type");
    zassert_equal(pkt->data_len, 0, "wrong data_len");
    zassert_is_null(pkt->data, "data should be NULL");

    destroy_packet(pkt);
}

ZTEST(sensor_node_suite, test_build_data_packet_with_payload)
{
    uint8_t payload[] = { 0x11, 0x22, 0x33, 0x44 };

    packet_t *pkt = build_packet(RESPONSE, payload, sizeof(payload));

    zassert_not_null(pkt, "packet should not be NULL");
    zassert_equal(pkt->type, RESPONSE, "wrong packet type");
    zassert_equal(pkt->data_len, sizeof(payload), "wrong data_len");
    zassert_not_null(pkt->data, "data should not be NULL");

    zassert_mem_equal(pkt->data, payload, sizeof(payload),
                      "payload contents were not copied correctly");

    destroy_packet(pkt);
}

ZTEST(sensor_node_suite, test_build_packet_copies_payload_not_alias)
{
    uint8_t payload[] = { 1, 2, 3 };

    packet_t *pkt = build_packet(RESPONSE, payload, sizeof(payload));
    zassert_not_null(pkt, "packet should not be NULL");

    payload[0] = 99;

    zassert_equal(((uint8_t *)pkt->data)[0], 1,
                  "packet should keep original copied data");

    destroy_packet(pkt);
}

ZTEST(sensor_node_suite, test_build_packet_null_payload_nonzero_len_fails)
{
    packet_t *pkt = build_packet(RESPONSE, NULL, 4);

    zassert_is_null(pkt, "packet should fail with NULL payload and nonzero length");
}

ZTEST(sensor_node_suite, test_destroy_packet_null_safe)
{
    destroy_packet(NULL);
    zassert_true(true, "destroy_packet(NULL) should not crash");
}

ZTEST(sensor_node_suite, test_packet_queue_starts_empty)
{
    packet_t *pkt = NULL;
    int ret = k_msgq_get(&packet_queue, &pkt, K_NO_WAIT);

    zassert_equal(ret, -ENOMSG, "packet queue should be empty");
}


ZTEST(sensor_node_suite, test_packet_queue_put_and_get)
{
    packet_t *pkt = build_packet(REQUEST, NULL, 0);
    zassert_not_null(pkt, "packet should not be NULL");

    int ret = k_msgq_put(&packet_queue, &pkt, K_NO_WAIT);
    zassert_equal(ret, 0, "k_msgq_put failed");

    packet_t *out = NULL;
    ret = k_msgq_get(&packet_queue, &out, K_NO_WAIT);

    zassert_equal(ret, 0, "k_msgq_get failed");
    zassert_equal(out, pkt, "queue returned wrong packet pointer");

    destroy_packet(out);
}

ZTEST(sensor_node_suite, test_packet_queue_fifo_order)
{
    packet_t *pkt1 = build_packet(REQUEST, NULL, 0);
    packet_t *pkt2 = build_packet(RESPONSE, NULL, 0);

    zassert_not_null(pkt1, "pkt1 should not be NULL");
    zassert_not_null(pkt2, "pkt2 should not be NULL");

    zassert_equal(k_msgq_put(&packet_queue, &pkt1, K_NO_WAIT), 0);
    zassert_equal(k_msgq_put(&packet_queue, &pkt2, K_NO_WAIT), 0);

    packet_t *out1 = NULL;
    packet_t *out2 = NULL;

    zassert_equal(k_msgq_get(&packet_queue, &out1, K_NO_WAIT), 0);
    zassert_equal(k_msgq_get(&packet_queue, &out2, K_NO_WAIT), 0);

    zassert_equal(out1, pkt1, "queue did not preserve FIFO order");
    zassert_equal(out2, pkt2, "queue did not preserve FIFO order");

    destroy_packet(out1);
    destroy_packet(out2);
}

ZTEST(sensor_node_suite, test_emergency_isr_gives_semaphore)
{
    int ret;

    ret = k_sem_take(&emergency_sem, K_NO_WAIT);
    zassert_equal(ret, -EBUSY, "emergency_sem should start empty");

    sensor_emergency_isr();

    ret = k_sem_take(&emergency_sem, K_NO_WAIT);
    zassert_equal(ret, 0, "sensor_emergency_isr should give emergency_sem");
}

ZTEST(sensor_node_suite, test_emergency_isr_does_not_overflow_binary_sem)
{
    sensor_emergency_isr();
    sensor_emergency_isr();
    sensor_emergency_isr();

    zassert_equal(k_sem_take(&emergency_sem, K_NO_WAIT), 0,
                  "first semaphore take should succeed");

    zassert_equal(k_sem_take(&emergency_sem, K_NO_WAIT), -EBUSY,
                  "binary semaphore should not exceed limit 1");
}

ZTEST(sensor_node_suite, test_packet_queue_initially_empty)
{
    packet_t *pkt = NULL;

    int ret = k_msgq_get(&packet_queue, &pkt, K_NO_WAIT);

    zassert_equal(ret, -ENOMSG, "packet_queue should be empty");
}

ZTEST(sensor_node_suite, test_packet_queue_put_get_single_packet)
{
    packet_t *pkt = build_packet(EMERGENCY, NULL, 0);
    zassert_not_null(pkt, "build_packet failed");

    zassert_equal(k_msgq_put(&packet_queue, &pkt, K_NO_WAIT), 0,
                  "k_msgq_put failed");

    packet_t *out = NULL;
    zassert_equal(k_msgq_get(&packet_queue, &out, K_NO_WAIT), 0,
                  "k_msgq_get failed");

    zassert_equal(out, pkt, "queue returned wrong packet pointer");
    zassert_equal(out->type, EMERGENCY, "wrong packet type");

    destroy_packet(out);
}

ZTEST(sensor_node_suite, test_build_config_packet_medium)
{
    uint8_t payload[] = { MEDIUM };

    packet_t *pkt = build_packet(CONFIG, payload, sizeof(payload));

    zassert_not_null(pkt, "packet should not be NULL");
    zassert_equal(pkt->type, CONFIG, "wrong packet type");
    zassert_equal(pkt->data_len, 1, "wrong data length");
    zassert_not_null(pkt->data, "data should not be NULL");
    zassert_equal(((uint8_t *)pkt->data)[0], MEDIUM, "wrong config value");

    destroy_packet(pkt);
}

ZTEST(sensor_node_suite, test_build_config_packet_high)
{
    uint8_t payload[] = { HIGH };

    packet_t *pkt = build_packet(CONFIG, payload, sizeof(payload));

    zassert_not_null(pkt, "packet should not be NULL");
    zassert_equal(pkt->type, CONFIG, "wrong packet type");
    zassert_equal(pkt->data_len, 1, "wrong data length");
    zassert_not_null(pkt->data, "data should not be NULL");
    zassert_equal(((uint8_t *)pkt->data)[0], HIGH, "wrong config value");

    destroy_packet(pkt);
}

ZTEST(sensor_node_suite, test_build_packet_empty_payload_pointer_ignored)
{
    uint8_t payload[] = { 0xAA, 0xBB };

    packet_t *pkt = build_packet(RESPONSE, payload, 0);

    zassert_not_null(pkt, "packet should not be NULL");
    zassert_equal(pkt->type, RESPONSE, "wrong packet type");
    zassert_equal(pkt->data_len, 0, "wrong data length");
    zassert_is_null(pkt->data, "zero-length payload should produce NULL data");

    destroy_packet(pkt);
}

static void *suite_setup(void)
{
    k_sleep(K_SECONDS(3));
    return NULL;
}

static void clear_packet_queue(void)
{
    packet_t *pkt;

    while (k_msgq_get(&packet_queue, &pkt, K_NO_WAIT) == 0) {
        if (pkt != NULL) {
            destroy_packet(pkt);
        }
    }
}

static void before_each(void *fixture)
{
    ARG_UNUSED(fixture);

    clear_packet_queue();

    while (k_sem_take(&emergency_sem, K_NO_WAIT) == 0) {}

    dynamic_set_variable = 0;
}

ZTEST_SUITE(sensor_node_suite, NULL, suite_setup, before_each, NULL, NULL);

void test_main(void)
{
    ztest_run_all(NULL, false, 1, 1);
}