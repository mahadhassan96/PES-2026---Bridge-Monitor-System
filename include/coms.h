#ifndef COMS_H
#define COMS_H
#define SYNC_BYTE 0xAA

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <zephyr/kernel.h>
#include <zephyr/drivers/uart.h>


typedef enum
{
    SYN,
    ACK,    
    READY,
    REQUEST,
    RESPONSE,
    EMERGENCY,
    EMERGENCY_ACK,
    CONFIG,
    CONFIG_ACK
} packet_type_t;
typedef struct
{
    uint8_t data_len; // the payload length in bytes
    packet_type_t type;
    uint8_t *data;
} packet_t;

packet_t *build_packet(packet_type_t type, uint8_t *data, uint8_t data_len);
void destroy_packet(packet_t *packet);

void send_packet(const struct device *uart_dev, packet_t *packet);
packet_t *receive_packet(const struct device *uart_dev, uint8_t payload_len);

void print_packet(const char* src_tag, const char *packet_type_tag, packet_t *packet);

void send_response(packet_type_t type, uint8_t *data, uint8_t data_len);

const char *packet_type_to_str(packet_type_t type);

#endif /* COMS_H */