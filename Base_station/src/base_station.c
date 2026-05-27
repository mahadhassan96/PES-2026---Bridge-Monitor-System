#include "base_station.h"
#include "coms.h"
#include "debug_print.h"
#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include "drivers/sensor_node/sensor_node.h"
#include <stdint.h>

// Queues
// Create an event queue.
K_MSGQ_DEFINE(event_queue, sizeof(base_station_event_t), QUEUE_SIZE, __alignof__(base_station_event_t));

// Raw byte queue: ISR to uart_rx_task
K_MSGQ_DEFINE(uart_byte_queue, sizeof(uint8_t), 256, 1);

// Packet queue: uart_rx_task to packet_handler_task
K_MSGQ_DEFINE(packet_queue, sizeof(packet_t *), QUEUE_SIZE, __alignof__(packet_t *));
base_station_t bs;

// Create macro aliases for each node.
#define BTN0_NODE DT_ALIAS(button0)
#define BTN1_NODE DT_ALIAS(button1)
#define BTN2_NODE DT_ALIAS(button2)

#define EMERGENCY_LED_NODE DT_ALIAS(led0)

#define UART_NODE DT_NODELABEL(uart0)

// Create specs for each node.
static const struct gpio_dt_spec btn0_spec = GPIO_DT_SPEC_GET(BTN0_NODE, gpios);
static const struct gpio_dt_spec btn1_spec = GPIO_DT_SPEC_GET(BTN1_NODE, gpios);
static const struct gpio_dt_spec btn2_spec = GPIO_DT_SPEC_GET(BTN2_NODE, gpios);
static const struct gpio_dt_spec emergency_led_spec = GPIO_DT_SPEC_GET(EMERGENCY_LED_NODE, gpios);

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);

static struct k_work_delayable emergency_ack_work;

// Create callback struct for button ISRs.
static struct gpio_callback reset_btn_cb_data;
static struct gpio_callback ack_btn_cb_data;
static struct gpio_callback sens_btn_cb_data;

static struct k_poll_signal poll_signal;
static struct k_timer request_timer;
static struct k_timer emergency_led_timer;

static struct k_poll_event events[1];

void init_btn(const struct gpio_dt_spec *spec, gpio_callback_handler_t callback, struct gpio_callback *callback_data)
{
    // Configures button pin as input.
    gpio_pin_configure_dt(spec, GPIO_INPUT);

    // Registers interrupt on edge to active level.
    gpio_pin_interrupt_configure_dt(spec, GPIO_INT_EDGE_TO_ACTIVE);

    // Initializes callback struct with ISR and the pins on which ISR should trigger.
    gpio_init_callback(callback_data, callback, BIT(spec->pin));

    // Adds ISR callback to the device.
    gpio_add_callback_dt(spec, callback_data);
}

void init_led(const struct gpio_dt_spec *spec)
{
    if (!gpio_is_ready_dt(spec))
    {
        return;
    }

    // Configures LED pin as output.
    int ret = gpio_pin_configure_dt(spec, GPIO_OUTPUT_INACTIVE);
    if (ret < 0)
    {
        return;
    }
}

// Enqueue newly received bytes in uart_byte_queue.
static void uart_isr(const struct device *dev, void *user_data)
{
    if (!uart_irq_update(dev))    { return; }
    if (!uart_irq_rx_ready(dev))  { return; }

    uint8_t byte;
    while (uart_fifo_read(dev, &byte, 1) == 1) {
        // Non-blocking put; drop byte on overflow (better than blocking in ISR)
        k_msgq_put(&uart_byte_queue, &byte, K_NO_WAIT);
    }
}

void emergency_ack_handler(struct k_work *work)
{
    APP_PRINT(BS_TAG, DEBUG_TAG, "Sending EMERGENCY_ACK response!");
    send_response(EMERGENCY_ACK, NULL, 0);
}

//Setup BTN ISRs
void reset_btn_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // Add reset pressed event to the event queue.
    base_station_event_t evt = RESET_PRESSED;
    k_msgq_put(&event_queue, &evt, K_NO_WAIT);
}

void ack_btn_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // Only allow sending the ACK if we are actually in an emergency
    if (bs.emergency) 
    {
        // Schedule the work immediately (K_NO_WAIT) to safely send the UART packet outside the ISR
        k_work_schedule(&emergency_ack_work, K_NO_WAIT);
    }
}

void sens_btn_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // Cycle through sensitivity levels on each press.
    base_station_event_t evt = SENS_PRESSED;
    k_msgq_put(&event_queue, &evt, K_NO_WAIT);
}

void init_defaults(base_station_t *bs)
{
    bs->curr_state = BOOT;
    bs->hw_init = true;
    bs->emergency = false;
    bs->emergency_light = false;

    // Zero out the readings.
    bs->sensor_readings.dist = 0;
    bs->sensor_readings.force = 0;
    bs->sensor_readings.accel_x = 0;
    bs->sensor_readings.accel_y = 0;
    bs->sensor_readings.accel_z = 0;

    // Begin with medium sensitivity by default.
    bs->sensitivity = MEDIUM;
}

// Sets up the initial state and performs necessary setup for the base station.
void init(base_station_t *bs)
{
    // Initialize the base station state
    APP_PRINT(BS_TAG, DEBUG_TAG, "Bootstrapping base station...");

    if (!bs->hw_init)
    {
        // Initialize UART device.
        if (!device_is_ready(uart_dev))
        {
            APP_PRINT(BS_TAG, ERROR_TAG, "UART device is not ready!");
            // Add error event to the event queue.
            base_station_event_t evt = ERROR_OCCURRED;
            k_msgq_put(&event_queue, &evt, K_NO_WAIT);
            return;
        }

        // Initilaze the sensor node device.
        bs->sn_dev = device_get_binding(SN_ALIAS);
        if (!device_is_ready(bs->sn_dev))
        {
            APP_PRINT(BS_TAG, ERROR_TAG, "Sensor node device not ready");
            return;
        }

        // Initialize buttons and their ISRs.
        init_btn(&btn0_spec, reset_btn_isr, &reset_btn_cb_data);
        init_btn(&btn1_spec, ack_btn_isr, &ack_btn_cb_data);
        init_btn(&btn2_spec, sens_btn_isr, &sens_btn_cb_data);

        // Initialize ISR
        uart_irq_callback_set(uart_dev, uart_isr);
        uart_irq_rx_enable(uart_dev);

        // Initialize LEDs.
        init_led(&emergency_led_spec);
    }

    // Default all other state values.
    init_defaults(bs);

    // Issue config packet to sensor node to set sensitivity.
    send_config(bs);

    // Initialize the polling signal and event.
    k_poll_signal_init(&poll_signal);
    k_poll_event_init(&events[0], K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &poll_signal);

    // Initialize the timer for periodic data requests, delayable work item for the emergency ack.
    k_timer_init(&request_timer, timer_handler, NULL);
    k_timer_init(&emergency_led_timer, led_timer_handler, NULL);
    k_work_init_delayable(&emergency_ack_work, emergency_ack_handler);

    // Add boot complete event to the event queue.
    base_station_event_t evt = BOOT_COMPLETE;
    k_msgq_put(&event_queue, &evt, K_NO_WAIT);
}

void timer_handler(struct k_timer *t)
{
    k_poll_signal_raise(&poll_signal, TIMER_SIGNAL);
}

void led_timer_handler(struct k_timer *timer)
{
    bs.emergency_light = !bs.emergency_light;
    gpio_pin_set_dt(&emergency_led_spec, bs.emergency_light);
}

static inline void toggle_timer(bool state_change, bool start)
{
    if (!state_change)
    {
        return;
    }
    if (start)
    {
        k_timer_start(&request_timer, K_SECONDS(WORK_INTERVAL_S), K_SECONDS(WORK_INTERVAL_S));
    }
    else
    {
        k_timer_stop(&request_timer);
    }
}

void error_handler(base_station_t *bs, bool state_change)
{
    // Stop the periodic timer during error.
    toggle_timer(state_change, false);

    // Turn on the emergency light solid during error.
    start_light();
}

void boot_handler(base_station_t *bs, bool state_change)
{
    toggle_timer(state_change, false);
    stop_light();

    // Re-boot the system.
    init(bs);
}

void alert_handler(base_station_t *bs, bool state_change)
{
    // Stop the periodic timer during alert.
    toggle_timer(state_change, false);

    // Flash the emergency light.
    start_flashing_light();
}

void send_config(base_station_t *bs)
{
    uint8_t config_data[1] = { bs->sensitivity };
    packet_t *packet = build_packet(CONFIG, config_data, sizeof(config_data));
    if (packet)
    {
        APP_PRINT(BS_TAG, DEBUG_TAG, "Sending config packet with sensitivity: %s", sensitivity_to_str(bs->sensitivity));

        send_packet(uart_dev, packet);
        destroy_packet(packet);
    }
    else
    {
        APP_PRINT(BS_TAG, ERROR_TAG, "Failed to build config packet!");
    }
}

void request_data()
{
    packet_t *packet = build_packet(REQUEST, NULL, 0);
    if (packet)
    {
        print_packet(BS_TAG, packet_type_to_str(REQUEST), packet);
        send_packet(uart_dev, packet);
        destroy_packet(packet);
    }
    else
    {
        APP_PRINT(BS_TAG, ERROR_TAG, "Packet build failed!");
    }
}

void normal_handler(base_station_t *bs, bool state_change)
{
    toggle_timer(state_change, true);
    stop_light();

    if (sensor_sample_fetch(bs->sn_dev) != 0 && bs->curr_state != NORMAL)
    {
        APP_PRINT(BS_TAG, ERROR_TAG, "BS fetch failed: not in normal state");
        return;
    }

    struct sensor_value dist, force, accel_x, accel_y, accel_z;
    sensor_channel_get(bs->sn_dev, SENSOR_CHAN_DISTANCE, &dist);
    sensor_channel_get(bs->sn_dev, SENSOR_CHAN_FORCE_N, &force);
    sensor_channel_get(bs->sn_dev, SENSOR_CHAN_ACCEL_X, &accel_x);
    sensor_channel_get(bs->sn_dev, SENSOR_CHAN_ACCEL_Y, &accel_y);
    sensor_channel_get(bs->sn_dev, SENSOR_CHAN_ACCEL_Z, &accel_z);

    update_sensor_values(&bs->sensor_readings, &dist, &force, &accel_x, &accel_y, &accel_z);

    sensor_reading_t *r;
    r = &bs->sensor_readings;
    APP_PRINT(BS_TAG, DEBUG_TAG, "Reading: { dist=%d, force=%d, accel=(%d, %d, %d) }",
        r->dist, r->force, r->accel_x, r->accel_y, r->accel_z);
}

void update_sensor_values(sensor_reading_t *readings, struct sensor_value *dist, struct sensor_value *force, struct sensor_value *accel_x, struct sensor_value *accel_y, struct sensor_value *accel_z)
{
    readings->dist = (int32_t)dist->val1;
    readings->force = (int32_t)force->val1;
    readings->accel_x = (int32_t)accel_x->val1;
    readings->accel_y = (int32_t)accel_y->val1;
    readings->accel_z = (int32_t)accel_z->val1;
}

void start_flashing_light(void)
{
    k_timer_start(&emergency_led_timer, K_NO_WAIT, K_MSEC(FLASH_MS));
}

void start_light(void)
{
    gpio_pin_set_dt(&emergency_led_spec, 1);
    bs.emergency_light = true;
}

void stop_light(void)
{
    k_timer_stop(&emergency_led_timer);
    gpio_pin_set_dt(&emergency_led_spec, 0);
    bs.emergency_light = false;
}

void worker_task()
{
    bs.hw_init = false;
    init(&bs);

    int signaled, result;
    bool state_change = false;

    while (true)
    {
        // Wait for a signal to perform work.
        k_poll(events, 1, K_FOREVER);

        if (events[0].state == K_POLL_STATE_SIGNALED)
        {

            // Check result & reset for the next signal.
            k_poll_signal_check(&poll_signal, &signaled, &result);
            k_poll_signal_reset(&poll_signal);

            state_change = (result == STATE_SIGNAL);

            switch (bs.curr_state)
            {
                case ALERT:
                    alert_handler(&bs, state_change);
                    break;

                case BOOT:
                    boot_handler(&bs, state_change);
                    break;

                case ERROR:
                    error_handler(&bs, state_change);
                    break;

                // Default to normal case.
                default:
                    normal_handler(&bs, state_change);
                    break;
            }
        }
    }
}

base_station_event_t get_next_state(base_station_state_t curr_state, base_station_event_t ev)
{
    switch (curr_state)
    {
    case NORMAL:
        if (ev == ANOMALY_DETECTED)
        {
            return ALERT;
        }
        else if (ev == RESET_PRESSED)
        {
            return BOOT;
        }
        else if (ev == ERROR_OCCURRED)
        {
            return ERROR;
        }
        break;

    case BOOT:
        if (ev == BOOT_COMPLETE)
        {
            return NORMAL;
        }
        break;

    case ALERT:
        if (ev == RESET_PRESSED)
        {
            return BOOT;
        }
        else if (ev == ANOMALY_CLEARED)
        {
            return NORMAL;
        }
        else if (ev == ERROR_OCCURRED)
        {
            return ERROR;
        }
        break;

    case ERROR:
        if (ev == RESET_PRESSED)
        {
            return BOOT;
        }
        break;
    }
    // If no transition occurs, return the current state.
    return curr_state;
}

const char *sensitivity_to_str(uint8_t sensitivity)
{
    switch (sensitivity)
    {
        case LOW: return "LOW";
        case MEDIUM: return "MEDIUM";
        case HIGH: return "HIGH";
        default: return "UNKNOWN";
    }
}


void process_packet(packet_t *packet)
{
    if (bs.emergency && packet->type != READY)
    {
        APP_PRINT(BS_TAG, DEBUG_TAG, "Already in emergency mode...please wait!");
        return;
    }

    switch (packet->type)
    {
        case RESPONSE:
        {
            if (packet->data != NULL && packet->data_len == sizeof(int32_t) * 5)
            {
                int32_t *readings = (int32_t *)packet->data;
                sensor_node_store_response(
                    readings[4], /* dist mm    */
                    readings[3], /* force mN   */
                    readings[0], /* accel X mg */
                    readings[1], /* accel Y mg */
                    readings[2]  /* accel Z mg */
                );
            }
            else
            {
                APP_PRINT(BS_TAG, ERROR_TAG, "Response payload missing or wrong size");
            }

            break;
        }
case EMERGENCY:
        {
            print_packet(BS_TAG, packet_type_to_str(EMERGENCY), packet);

            if (!bs.emergency) {
                bs.emergency = true;
                base_station_event_t evt = ANOMALY_DETECTED;
                k_msgq_put(&event_queue, &evt, K_NO_WAIT);
            }
            APP_PRINT(BS_TAG, DEBUG_TAG, "ALERT MODE! Press BTN 'GP21' to clear emergency and send EMERGENCY_ACK.");
            
            break;
        }

        case READY:
        {
            bs.emergency = false;
            base_station_event_t evt = ANOMALY_CLEARED;
            k_msgq_put(&event_queue, &evt, K_NO_WAIT);
            break;
        }

        default:
            print_packet(BS_TAG, "DEFAULT", packet);
            break;
        }
}

//uart_rx_task: frames bytes - complete packetS
void uart_rx_task(void)
{
    static uint8_t packet_type  = 0;
    static uint8_t payload_len  = 0;
    static uint8_t payload_idx  = 0;
    static uint8_t payload[64];
    static int     state        = 0;

    uint8_t byte;

    while (true) {
        // Block until a byte arrives from the ISR
        k_msgq_get(&uart_byte_queue, &byte, K_FOREVER);

        switch (state) {
        case 0: // WAIT_SYNC
            if (byte == SYNC_BYTE) {
                payload_idx = 0;
                state = 1;
            }
            break;

        case 1: // WAIT_TYPE
            packet_type = byte;
            state = 2;
            break;

        case 2: // WAIT_LENGTH
            payload_len = byte;

            if (payload_len == 0)
            {
                // Zero-payload packet — hand it off immediately
                packet_t *pkt = build_packet(packet_type, NULL, 0);
                if (pkt)
                {
                    if (k_msgq_put(&packet_queue, &pkt, K_NO_WAIT) != 0)
                    {
                        APP_PRINT(BS_TAG, ERROR_TAG, "Packet queue full; dropping packet!");
                        destroy_packet(pkt);
                    }
                }
                state = 0;
            }
            else if (payload_len > sizeof(payload))
            {
                APP_PRINT(BS_TAG, ERROR_TAG, "Payload too large (%u); dropping packet!", payload_len);
                state = 0;
            }
            else
            {
                payload_idx = 0;
                state = 3;
            }
            break;

        case 3: // WAIT_PAYLOAD
            payload[payload_idx++] = byte;
            if (payload_idx >= payload_len)
            {
                packet_t *pkt = build_packet(packet_type, payload, payload_len);
                if (pkt)
                {
                    if (k_msgq_put(&packet_queue, &pkt, K_NO_WAIT) != 0)
                    {
                        APP_PRINT(BS_TAG, ERROR_TAG, "Packet queue full; dropping packet!");
                        destroy_packet(pkt);
                    }
                }
                state = 0;
            }
            break;

        default:
            state = 0;
            break;
        }
    }
}

/**
 * @brief Dequeues complete packets and processes them.
 */
void packet_handler_task(void)
{
    packet_t *pkt;

    while (true)
    {
        if (k_msgq_get(&packet_queue, &pkt, K_FOREVER) == 0) 
        {
            process_packet(pkt);
            destroy_packet(pkt);
        }
    }
}

void cycle_sensitivity(base_station_t *bs)
{
    bs->sensitivity = (bs->sensitivity + 1) % SENSITIVITY_LEVELS;
}

// Periodically checks the event queue and updates the base station state accordingly.
void fsm_task()
{
    base_station_event_t ev;

    while (true)
    {
        // Check for events in the event queue and handle them.
        if (k_msgq_get(&event_queue, &ev, K_FOREVER) == 0)
        {

            log_event(&bs, ev);

            if (ev == SENS_PRESSED)
            {
                // Cycle through sensitivity levels and send config packet, without changing the current state.
                bs.sensitivity = (bs.sensitivity + 1) % SENSITIVITY_LEVELS;
                cycle_sensitivity(&bs);
                send_config(&bs);
                continue;
            }

            // Advance state based on the event.
            bs.curr_state = get_next_state(bs.curr_state, ev);
            k_poll_signal_raise(&poll_signal, STATE_SIGNAL);
            log_state(&bs);
        }
    }
}

const char *event_to_str(base_station_event_t ev)
{
    switch (ev)
    {
    case RESET_PRESSED:
        return "Reset Pressed";
    case SENS_PRESSED:
        return "Sensitivity Pressed";
    case BOOT_COMPLETE:
        return "Boot Complete";
    case ANOMALY_DETECTED:
        return "Anomaly Detected";
    case ANOMALY_CLEARED:
        return "Anomaly Cleared";
    case ERROR_OCCURRED:
        return "Error Occurred";
    default:
        return "Unknown Event";
    }
}

void log_event(base_station_t *bs, base_station_event_t ev)
{
    APP_PRINT(BS_TAG, DEBUG_TAG, "Event: %s", event_to_str(ev));
}

const char *state_to_str(base_station_state_t state)
{
    switch (state)
    {
    case NORMAL:
        return "NORMAL";
    case ALERT:
        return "ALERT";
    case BOOT:
        return "BOOT";
    case ERROR:
        return "ERROR";
    default:
        return "UNKNOWN";
    }
}

void log_state(base_station_t *bs)
{
    APP_PRINT(BS_TAG, DEBUG_TAG, "State: %s", state_to_str(bs->curr_state));
}


#ifndef CONFIG_ZTEST
K_THREAD_DEFINE(fsm_tid, STACK_SIZE, fsm_task, NULL, NULL, NULL, UPDATE_PRIO, 0, 0);
K_THREAD_DEFINE(worker_tid, STACK_SIZE, worker_task, NULL, NULL, NULL, WORKER_PRIO, 0, 0);
K_THREAD_DEFINE(rx_tid, STACK_SIZE, uart_rx_task, NULL, NULL, NULL, READ_PRIO, 0, 0);
K_THREAD_DEFINE(handler_tid, STACK_SIZE, packet_handler_task, NULL, NULL, NULL, HANDLER_PRIO, 0, 0);
#endif