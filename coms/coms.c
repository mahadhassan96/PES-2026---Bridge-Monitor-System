#include "../../include/coms.h"

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
    if (packet == NULL) {
        return NULL;
    }
    packet->type = type;
    packet->data_len = data_len;

    // Allocate memory for the data and copy the data.
    packet->data = malloc(data_len);
    memcpy(packet->data, data, data_len);

    return packet;
}

/*
 * Destroy a packet and free all allocated memory.
 *
 * Args:
 *   @param packet pointer to the packet to destroy
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
 * Args:
 *   @param uart_dev pointer to the UART device to send bytes through
 *   @param data pointer to the data to send
 *   @param len the number of bytes to send
 */
static void uart_send_bytes(const struct device *uart_dev, uint8_t *data, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        uart_poll_out(uart_dev, data[i]);
    }
}

/*
 * Send a packet over the specified UART device.
 *
 * Args:
 *   @param uart_dev pointer to the UART device to send the packet through
 *   @param packet pointer to the packet to send
 */
void send_packet(const struct device *uart_dev, packet_t *packet)
{
    // Send the data length first.
    uart_send_bytes(uart_dev, (uint8_t *)&packet->data_len, sizeof(uint8_t));

    // Then send the packet type.
    uart_send_bytes(uart_dev, (uint8_t *)&packet->type, sizeof(packet_type_t));

    // Finally send the data.
    uart_send_bytes(uart_dev, packet->data, packet->data_len);
}

/*
 * Receive bytes from the specified UART device.
 *
 * Args:
 *   @param uart_dev pointer to the UART device to receive bytes from
 *   @param buffer pointer to the buffer to store the received bytes
 *   @param len the number of bytes to receive
 */
static void uart_receive_bytes(const struct device *uart_dev, uint8_t *buffer, uint32_t len)
{
    for (uint32_t i = 0; i < len; i++) {
        buffer[i] = uart_poll_in(uart_dev);
    }
}

/*
 * Reads packet type and payload. Assumes payload_len has already been read
 * from the UART stream.
 *
 * Args:
 *   @param uart_dev pointer to the UART device to receive the packet from
 *   @param data_len the length of the data payload in bytes
 *
 * Returns:
 *   pointer to the received packet (caller is responsible for freeing memory)
*/
packet_t *receive_packet(const struct device *uart_dev, uint8_t data_len)
{
    packet_t *packet = malloc(sizeof(packet_t));
    if (packet == NULL) {
        return NULL;
    }

    // Always start by receiving the packet type.
    uart_receive_bytes(uart_dev, (uint8_t *)&packet->type, sizeof(packet_type_t));

    // Receive the data based on the provided data length.
    packet->data = malloc(data_len);
    if (packet->data == NULL) {
        free(packet);
        return NULL;
    }

    uart_receive_bytes(uart_dev, packet->data, data_len);
    packet->data_len = data_len;
    return packet;
}