#include "../../include/base_station.h"

// Create an event queue.
K_MSGQ_DEFINE(event_queue, sizeof(base_station_event_t), QUEUE_SIZE, __alignof__(base_station_event_t));

base_station_t bs;

// Create a semaphore for logging.
// K_SEM_DEFINE(logging_sem, 0, 1);

// Create macro aliases for each node.
#define BTN0_NODE DT_ALIAS(button0)
#define BTN1_NODE DT_ALIAS(button1)

// Create specs for each node.
static const struct gpio_dt_spec btn0_spec = GPIO_DT_SPEC_GET(BTN0_NODE, gpios);
static const struct gpio_dt_spec btn1_spec = GPIO_DT_SPEC_GET(BTN1_NODE, gpios);

// Create callback struct for button ISRs.
static struct gpio_callback reset_btn_cb_data;
static struct gpio_callback logging_btn_cb_data;

static struct k_poll_signal poll_signal;
static struct k_timer request_timer;

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

// Sets up the initial state and performs necessary setup for the base station.
void init(base_station_t* bs)
{
    // Initialize the base station state
    bs->curr_state = BOOT;
    bs->logging = false;

    // Initialize sensors and perform handshake
    // init_sensors();
    // sensor_handshake();

    // Initialize buttons and their ISRs.
    init_btn(&btn0_spec, reset_btn_isr, &reset_btn_cb_data);
    init_btn(&btn1_spec, logging_btn_isr, &logging_btn_cb_data);

    // Add boot complete event to the event queue.
    base_station_event_t evt = BOOT_COMPLETE;
    k_msgq_put(&event_queue, &evt, K_NO_WAIT);
}

void reset_btn_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // Add reset pressed event to the event queue.
    base_station_event_t evt = RESET_PRESSED;
    k_msgq_put(&event_queue, &evt, K_NO_WAIT);
}

void logging_btn_isr(const struct device *dev, struct gpio_callback *cb, uint32_t pins)
{
    // Toggle the logging state.
    base_station_event_t evt = LOGGING_PRESSED;
    k_msgq_put(&event_queue, &evt, K_NO_WAIT);
}

void timer_handler(struct k_timer *t)
{
    // k_poll_signal_raise(&poll_signal, 1);
}

void worker_task()
{
    init(&bs);
    struct k_poll_event events[1];

    // k_poll_signal_init(&poll_signal);
    // k_poll_event_init(&events[0], K_POLL_TYPE_SIGNAL, K_POLL_MODE_NOTIFY_ONLY, &poll_signal);
    k_timer_init(&request_timer, timer_handler, NULL);
    k_timer_start(&request_timer, K_SECONDS(5), K_SECONDS(5));
    int signaled, result;

    // while (true) {
    //     k_poll(events, 1, K_FOREVER);

    //     if (events[0].state == K_POLL_STATE_SIGNALED) {

    //         printk("Doing work!\n");
            
    //         k_poll_signal_check(&poll_signal, &signaled, &result);
    //         k_poll_signal_reset(&poll_signal);

    //         // if (result == 1) {
    //         //     printk("DING!\n");
    //         // } else {
    //         //     printk("State updated!\n");
    //         // }

    //         // switch (bs.curr_state)
    //         // {
    //         //     case ALERT:
    //         //         /* code */
    //         //         break;

    //         //     case BOOT:
    //         //         printk("Booting!\n");
    //         //         // Re-initialize the system on boot.
    //         //         init(&bs);
    //         //         break;

    //         //     case ERROR:
    //         //         break;
                
    //         //     // Default to normal case.
    //         //     default:
    //         //         break;
    //         // }
    //     }

    //     events[0].state = K_POLL_STATE_NOT_READY;
    // }
}

base_station_event_t get_next_state(base_station_state_t curr_state, base_station_event_t ev)
{
    switch (curr_state)
    {
        case NORMAL:
            if (ev == ANOMALY_DETECTED) { return ALERT; }
            else if(ev == RESET_PRESSED) { return BOOT; }
            else if (ev == ERROR_OCCURRED) { return ERROR; }
            break;

        case BOOT:
            if (ev == BOOT_COMPLETE) { return NORMAL; }
            break;

        case ALERT:
            if(ev == RESET_PRESSED) { return BOOT; }
            else if (ev == ANOMALY_CLEARED) { return NORMAL; }
            else if (ev == ERROR_OCCURRED) { return ERROR; }
            break;

        case ERROR:
            if(ev == RESET_PRESSED) { return BOOT; }
            break;
    }
    // If no transition occurs, return the current state.
    return curr_state;
}

// Periodically checks the event queue and updates the base station state accordingly. 
void fsm_task()
{
    base_station_event_t ev;

    while (true) {
        // Check for events in the event queue and handle them.
        if (k_msgq_get(&event_queue, &ev, K_FOREVER) == 0) {

            log_event(&bs, ev);

            if (ev == LOGGING_PRESSED) {
                // Toggle logging state without changing the current state.
                bs.logging = !bs.logging;
                log_state(&bs);
                continue;
            }

            // Advance state based on the event.
            bs.curr_state = get_next_state(bs.curr_state, ev);
            // k_poll_signal_raise(&poll_signal, 0);
            log_state(&bs);
        }
    }
}

static const char *event_str(base_station_event_t ev)
{
    switch (ev) {
        case RESET_PRESSED:     return "Reset Pressed";
        case LOGGING_PRESSED:   return "Logging Pressed";
        case BOOT_COMPLETE:     return "Boot Complete";
        case ANOMALY_DETECTED:  return "Anomaly Detected";
        case ANOMALY_CLEARED:   return "Anomaly Cleared";
        case ERROR_OCCURRED:    return "Error Occurred";
        default:                return "Unknown Event";
    }
}

void log_event(base_station_t* bs, base_station_event_t ev)
{
    const char *msg = event_str(ev);

    // Always log logging events.
    if (ev == LOGGING_PRESSED)
    {
        printk("[DEBUG] Event: %s\n", msg);
        return;
    }

    // Skip printing if logging is disabled.
    if (!bs->logging)
    {
        return;
    }

    printk("[DEBUG] Event: %s\n", msg);
}

static const char *state_str(base_station_state_t state)
{
    switch (state) {
        case NORMAL:    return "NORMAL";
        case ALERT:     return "ALERT";
        case BOOT:      return "BOOT";
        case ERROR:     return "ERROR";
        default:        return "UNKNOWN";
    }
}

void log_state(base_station_t* bs)
{
    const char *msg = state_str(bs->curr_state);

    // Skip printing if logging is disabled.
    if(!bs->logging) { return; }

    printk("[DEBUG] State: %s\n", msg);
}

K_THREAD_DEFINE(fsm_tid, STACK_SIZE, fsm_task, NULL, NULL, NULL, UPDATE_PRIO, 0, 0);
K_THREAD_DEFINE(worker_tid, STACK_SIZE, worker_task, NULL, NULL, NULL, WORKER_PRIO, 0, 0);