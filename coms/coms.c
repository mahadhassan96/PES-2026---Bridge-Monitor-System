#include "../include/coms.h"

// Mutex for UART channel access.
K_MUTEX_DEFINE(uart_tx_mutex);
#define UART_NODE DT_NODELABEL(uart0)

static const struct device *uart_dev = DEVICE_DT_GET(UART_NODE);


/*
 * Build a new packet with the specified type and payload.
 * The caller is responsible for freeing the memory allocated for the packet.
 * 
 * NOTE: maximum payload size is 255 bytes.
 * 
 * @param type the type of the packet
 * @param data pointer to the data (new packet will copy this data)
 * @param data_len the length of the data in bytes
 */
packet_t *build_packet(packet_type_t type, uint8_t *data, uint8_t data_len)
{
    packet_t *packet = malloc(sizeof(packet_t));
    if (packet == NULL) 
    {
        return NULL;
    }
    packet->type = type;
    packet->data_len = data_len;


    // Allocate memory for the data and copy the data.
    packet->data = NULL;

    if (data_len > 0) {
        if (data == NULL)
        {
            free(packet);
            return NULL;
        }

        packet->data = malloc(data_len);
        if (packet->data == NULL)
        {
            free(packet);
            return NULL;
        }
        memcpy(packet->data, data, data_len);
    }

    return packet;
}

/*
 * Destroy a packet and free all allocated memory.
 *
 * @param packet pointer to the packet to destroy
 */
void destroy_packet(packet_t *packet)
{
    if (packet) {
        if(packet->data != NULL) 
        {
            free(packet->data);
        }
        free(packet);
    }
}

/*
 * Send bytes over the specified UART device.
 *
 * @param uart_dev pointer to the UART device to send bytes through
 * @param data pointer to the data to send
 * @param len the number of bytes to send
 */
static void uart_send_bytes(const struct device *uart_dev, uint8_t *data, uint32_t len)
{

    for (uint32_t i = 0; i < len; i++) {
        uart_poll_out(uart_dev, data[i]);
    }
    k_sleep(K_MSEC(1));

}

/*
 * Send a packet over the specified UART device.
 * Locks the UART channel mutex to ensure exclusive access during transmission.
 *
 * @param uart_dev pointer to the UART device to send the packet through
 * @param packet pointer to the packet to send
 */
void send_packet(const struct device *uart_dev, packet_t *packet)
{
    k_mutex_lock(&uart_tx_mutex, K_FOREVER);

    // Send packet start byte.
    uint8_t sync = SYNC_BYTE;
    uart_send_bytes(uart_dev, &sync, 1);
    k_sleep(K_MSEC(1));

    // Send the packet type.
    uart_send_bytes(uart_dev, (uint8_t *)&packet->type, 1);
    k_sleep(K_MSEC(1));

    // then Send the data length
    uart_send_bytes(uart_dev, (uint8_t *)&packet->data_len, 1);
    k_sleep(K_MSEC(1));

    // Finally send the data (if there is data).
    if (packet->data_len > 0)
    {
        uart_send_bytes(uart_dev, packet->data, packet->data_len);
    }

    k_mutex_unlock(&uart_tx_mutex);
}

/*
 * Receive bytes from the specified UART device.
 *
 * @param uart_dev pointer to the UART device to receive bytes from
 * @param buffer pointer to the buffer to store the received bytes
 * @param len the number of bytes to receive
 */
static void uart_receive_bytes(const struct device *uart_dev, uint8_t *buffer, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) 
    {
        while (uart_poll_in(uart_dev, &buffer[i]) != 0)
        {
            k_sleep(K_MSEC(1));
        }
    }
}

/*
 * Reads packet type and payload. Assumes payload_len has already been read
 * from the UART stream.
 *
 * @param uart_dev pointer to the UART device to receive the packet from
 * @param data_len the length of the data payload in bytes
 *
 * @returns pointer to the received packet (caller is responsible for freeing memory)
*/
packet_t *receive_packet(const struct device *uart_dev, uint8_t data_len)
{
    packet_t *packet = malloc(sizeof(packet_t));
    if (packet == NULL)
    {
        return NULL;
    }

    // Always start by receiving the packet type.
    uart_receive_bytes(uart_dev, (uint8_t *)&packet->type, 1);

    // Receive the data based on the provided data length.
    packet->data = NULL;
    if (data_len > 0) {
        packet->data = malloc(data_len);
        if (packet->data == NULL) {
            free(packet);
            return NULL;
        }

        uart_receive_bytes(uart_dev, packet->data, data_len);
    }
    packet->data_len = data_len;
    return packet;
}


void print_packet(const char *tag, packet_t *packet)
{
    static const char *packet_type_names[] =
    {
        [SYN]           = "SYN",
        [ACK]           = "ACK",
        [REQUEST]       = "REQUEST",
        [RESPONSE]      = "RESPONSE",
        [EMERGENCY]     = "EMERGENCY",
        [EMERGENCY_ACK] = "EMERGENCY_ACK"
    };

    if (packet == NULL)
    {
        printk("\n[%s] ERROR: NULL packet\n", tag);
        return;
    }

    const char *type_str =
        (packet->type < ARRAY_SIZE(packet_type_names) && packet_type_names[packet->type])
        ? packet_type_names[packet->type]
        : "UNKNOWN";

    printk(
        "\n================ %s PACKET ================\n"
        "TYPE   : %s (%u / 0x%02X)\n"
        "LENGTH : %u\n"
        "PAYLOAD: ",
        tag,
        type_str,
        packet->type,
        packet->type,
        packet->data_len
    );

    if (packet->data_len == 0 || packet->data == NULL)
    {
        printk("<EMPTY>\n");
    }
    /* Check if the packet actually contains our 5-integer telemetry payload */
    else if ((packet->type == RESPONSE || packet->type == EMERGENCY) && 
             (packet->data_len == sizeof(int) * 5))
    {
        int *readings = (int *)packet->data;
        printk("\n"
               "  -> Accel X: %d\n"
               "  -> Accel Y: %d\n"
               "  -> Accel Z: %d\n"
               "  -> FSR : %d\n"
               "  -> DISTANCE: %d\n",
               readings[0], readings[1], readings[2], readings[3], readings[4]);
    }
    /* Fallback layout: Print raw hex dump for control or malformed packets */
    else
    {
        for (uint8_t i = 0; i < packet->data_len; i++)
        {
            printk("%02X ", packet->data[i]);
        }
        printk("\n");
    }
    
    printk("==================================================\n");
}

void send_response(packet_type_t type, uint8_t *data, uint8_t data_len)
{
    packet_t *response_packet = build_packet(type, data, data_len);
    if (response_packet)
    {
        print_packet("SENSOR NODE send_response", response_packet);
        send_packet(uart_dev, response_packet);
        destroy_packet(response_packet);
    }
}
