#include <Arduino.h>
#include <ESP8266WiFi.h>
#include "./wifi_structures.h"
extern "C"
{
#include "user_interface.h"
    typedef void (*freedom_outside_cb_t)(uint8 status);
    int wifi_register_send_pkt_freedom_cb(freedom_outside_cb_t cb);
    void wifi_unregister_send_pkt_freedom_cb(void);
    int wifi_send_pkt_freedom(uint8 *buf, int len, bool sys_seq);
}

#define MAX_CLIENTS_TRACKED 200
#define TYPE_MANAGEMENT 0x00
#define TYPE_CONTROL 0x01
#define TYPE_DATA 0x02
#define SUBTYPE_PROBE_REQUEST 0x04

int no_new_device = 0;
int clients_known_count = 0;
clientinfo clients_known[MAX_CLIENTS_TRACKED];

String formatMac1(uint8_t mac[ETH_MAC_LEN])
{
    String hi = "";
    for (int i = 0; i < ETH_MAC_LEN; i++)
    {
        if (mac[i] < 16)
            hi = hi + "0" + String(mac[i], HEX);
        else
            hi = hi + String(mac[i], HEX);
        if (i < 5)
            hi = hi + ":";
    }
    return hi;
}

int register_client(clientinfo &ci)
{
    int known = 0; // Clear known flag
    for (int u = 0; u < clients_known_count; u++)
    {
        if (!memcmp(clients_known[u].station, ci.station, ETH_MAC_LEN))
        {
            clients_known[u].rssi = ci.rssi;
            clients_known[u].lastDiscoveredTime = millis();
            known = 1;
            break;
        }
    }

    if (!known)
    {
        ci.lastDiscoveredTime = millis();
        memcpy(&clients_known[clients_known_count], &ci, sizeof(ci));
        clients_known_count++;
    }

    if ((unsigned int)clients_known_count >=
        sizeof(clients_known) / sizeof(clients_known[0]))
    {
        Serial.printf("exceeded max clients_known\n");
        clients_known_count = 0;
    }

    return known;
}

void promisc_cb(uint8_t *buf, uint16_t len)
{

    struct packetBuffer *snifferPacket = (struct packetBuffer *)buf;
    unsigned int frameControl = ((unsigned int)snifferPacket->buf[1] << 8) + snifferPacket->buf[0];

    uint8_t frameType = (frameControl & 0b0000000000001100) >> 2;
    uint8_t frameSubType = (frameControl & 0b0000000011110000) >> 4;

    // Only look for probe request packets
    if (frameType != TYPE_MANAGEMENT ||
        frameSubType != SUBTYPE_PROBE_REQUEST)
        return;

    if (len == 128)
    {
        if ((snifferPacket->buf[0] == 0x40))
        {
            struct clientinfo ci = parse_probe(snifferPacket->buf, 36, snifferPacket->rx_ctrl.rssi);
            if (register_client(ci) == 0)
            {
                no_new_device = 0;
            }
        }
    }
}