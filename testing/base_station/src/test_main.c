#include <zephyr/ztest.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/printk.h>
#include "../../../include/coms.h"
#include "../../../include/base_station.h"

extern base_station_t bs;
extern struct k_msgq event_queue;

ZTEST(base_station_suite, test_sensitivity_to_str_valid)
{
    zassert_str_equal(sensitivity_to_str(0), "LOW");
    zassert_str_equal(sensitivity_to_str(1), "MEDIUM");
    zassert_str_equal(sensitivity_to_str(2), "HIGH");
}

ZTEST(base_station_suite, test_sensitivity_to_str_invalid)
{
    zassert_str_equal(sensitivity_to_str(99), "UNKNOWN");
}

ZTEST(base_station_suite, test_packet_type_to_str_valid)
{
    zassert_str_equal(packet_type_to_str(REQUEST), "REQUEST");
    zassert_str_equal(packet_type_to_str(RESPONSE), "RESPONSE");
    zassert_str_equal(packet_type_to_str(EMERGENCY), "EMERGENCY");
    zassert_str_equal(packet_type_to_str(EMERGENCY_ACK), "EMERGENCY_ACK");
    zassert_str_equal(packet_type_to_str(READY), "READY");
}

ZTEST(base_station_suite, test_packet_type_to_str_invalid)
{
    zassert_str_equal(packet_type_to_str(0xFF), "UNKNOWN");
}

ZTEST(base_station_suite, test_get_next_state_boot_complete)
{
    zassert_equal(get_next_state(BOOT, BOOT_COMPLETE), NORMAL);
}

ZTEST(base_station_suite, test_get_next_state_normal_anomaly)
{
    zassert_equal(get_next_state(NORMAL, ANOMALY_DETECTED), ALERT);
}

ZTEST(base_station_suite, test_get_next_state_normal_reset)
{
    zassert_equal(get_next_state(NORMAL, RESET_PRESSED), BOOT);
}

ZTEST(base_station_suite, test_get_next_state_normal_error)
{
    zassert_equal(get_next_state(NORMAL, ERROR_OCCURRED), ERROR);
}

ZTEST(base_station_suite, test_get_next_state_alert_clear)
{
    zassert_equal(get_next_state(ALERT, ANOMALY_CLEARED), NORMAL);
}

ZTEST(base_station_suite, test_get_next_state_alert_reset)
{
    zassert_equal(get_next_state(ALERT, RESET_PRESSED), BOOT);
}

ZTEST(base_station_suite, test_get_next_state_alert_error)
{
    zassert_equal(get_next_state(ALERT, ERROR_OCCURRED), ERROR);
}

ZTEST(base_station_suite, test_get_next_state_error_reset)
{
    zassert_equal(get_next_state(ERROR, RESET_PRESSED), BOOT);
}

ZTEST(base_station_suite, test_get_next_state_ignored_event_stays_same)
{
    zassert_equal(get_next_state(NORMAL, SENS_PRESSED), NORMAL);
    zassert_equal(get_next_state(BOOT, RESET_PRESSED), BOOT);
    zassert_equal(get_next_state(ERROR, ANOMALY_DETECTED), ERROR);
}

ZTEST(base_station_suite, test_update_sensor_values_basic)
{
    sensor_reading_t readings = {0};

    struct sensor_value dist = { .val1 = 100, .val2 = 0 };
    struct sensor_value force = { .val1 = 200, .val2 = 0 };
    struct sensor_value ax = { .val1 = 1, .val2 = 0 };
    struct sensor_value ay = { .val1 = 2, .val2 = 0 };
    struct sensor_value az = { .val1 = 3, .val2 = 0 };

    update_sensor_values(&readings, &dist, &force, &ax, &ay, &az);

    zassert_equal(readings.dist, 100);
    zassert_equal(readings.force, 200);
    zassert_equal(readings.accel_x, 1);
    zassert_equal(readings.accel_y, 2);
    zassert_equal(readings.accel_z, 3);
}

ZTEST(base_station_suite, test_update_sensor_values_negative)
{
    sensor_reading_t readings = {0};

    struct sensor_value dist = { .val1 = -10, .val2 = 0 };
    struct sensor_value force = { .val1 = -20, .val2 = 0 };
    struct sensor_value ax = { .val1 = -1, .val2 = 0 };
    struct sensor_value ay = { .val1 = -2, .val2 = 0 };
    struct sensor_value az = { .val1 = -3, .val2 = 0 };

    update_sensor_values(&readings, &dist, &force, &ax, &ay, &az);

    zassert_equal(readings.dist, -10);
    zassert_equal(readings.force, -20);
    zassert_equal(readings.accel_x, -1);
    zassert_equal(readings.accel_y, -2);
    zassert_equal(readings.accel_z, -3);
}

ZTEST(base_station_suite, test_process_packet_emergency_sets_emergency)
{
    bs = (base_station_t){0};
    k_msgq_purge(&event_queue);

    packet_t pkt = {
        .type = EMERGENCY,
        .data = NULL,
        .data_len = 0,
    };

    bs.emergency = false;

    process_packet(&pkt);

    zassert_true(bs.emergency);
}

ZTEST(base_station_suite, test_process_packet_ready_clears_emergency)
{
    bs = (base_station_t){0};
    k_msgq_purge(&event_queue);

    packet_t pkt = {
        .type = READY,
        .data = NULL,
        .data_len = 0,
    };

    bs.emergency = true;

    process_packet(&pkt);

    zassert_false(bs.emergency);
}

ZTEST(base_station_suite, test_process_packet_response_wrong_size_no_crash)
{
    bs = (base_station_t){0};
    k_msgq_purge(&event_queue);

    uint8_t bad_payload[3] = {1, 2, 3};

    packet_t pkt = {
        .type = RESPONSE,
        .data = bad_payload,
        .data_len = sizeof(bad_payload),
    };

    bs.emergency = false;

    process_packet(&pkt);

    zassert_true(true);
}

ZTEST(base_station_suite, test_packet_build)
{
    bs = (base_station_t){0};
    k_msgq_purge(&event_queue);

    packet_t *pkt = build_packet(1, NULL, 0);

    zassert_not_null(pkt, "packet should not be NULL");

    destroy_packet(pkt);
}

ZTEST(base_station_suite, test_process_packet_emergency_enqueues_anomaly_detected)
{
    packet_t pkt = { .type = EMERGENCY, .data = NULL, .data_len = 0 };
    base_station_event_t ev;

    bs.emergency = false;
    k_msgq_purge(&event_queue);

    process_packet(&pkt);

    zassert_equal(k_msgq_get(&event_queue, &ev, K_NO_WAIT), 0);
    zassert_equal(ev, ANOMALY_DETECTED);
}

ZTEST(base_station_suite, test_process_packet_ready_enqueues_anomaly_cleared)
{
    packet_t pkt = { .type = READY, .data = NULL, .data_len = 0 };
    base_station_event_t ev;

    bs.emergency = true;
    k_msgq_purge(&event_queue);

    process_packet(&pkt);

    zassert_equal(k_msgq_get(&event_queue, &ev, K_NO_WAIT), 0);
    zassert_equal(ev, ANOMALY_CLEARED);
}

ZTEST(base_station_suite, test_reset_isr_enqueues_reset_pressed)
{
    base_station_event_t ev;

    k_msgq_purge(&event_queue);

    reset_btn_isr(NULL, NULL, 0);

    zassert_equal(k_msgq_get(&event_queue, &ev, K_NO_WAIT), 0);
    zassert_equal(ev, RESET_PRESSED);
}

ZTEST(base_station_suite, test_cycle_sensitivity_full_rotation)
{
    base_station_t bs = {0};

    bs.sensitivity = LOW;

    cycle_sensitivity(&bs);
    zassert_equal(bs.sensitivity, MEDIUM);

    cycle_sensitivity(&bs);
    zassert_equal(bs.sensitivity, HIGH);

    cycle_sensitivity(&bs);
    zassert_equal(bs.sensitivity, LOW);
}

ZTEST(base_station_suite, test_sens_isr_enqueues_sens_pressed)
{
    base_station_event_t ev;

    k_msgq_purge(&event_queue);

    sens_btn_isr(NULL, NULL, 0);

    zassert_equal(k_msgq_get(&event_queue, &ev, K_NO_WAIT), 0);
    zassert_equal(ev, SENS_PRESSED);
}

ZTEST(base_station_suite, test_process_packet_ignores_non_ready_during_emergency)
{
    packet_t pkt = { .type = RESPONSE, .data = NULL, .data_len = 0 };

    bs.emergency = true;
    k_msgq_purge(&event_queue);

    process_packet(&pkt);

    zassert_true(bs.emergency);
    zassert_not_equal(k_msgq_get(&event_queue, &(base_station_event_t){0}, K_NO_WAIT), 0);
}

static void *suite_setup(void)
{
    k_sleep(K_SECONDS(3));
    return NULL;
}

ZTEST_SUITE(base_station_suite, NULL, suite_setup, NULL, NULL, NULL);