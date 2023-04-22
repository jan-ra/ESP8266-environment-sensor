#include <Arduino.h>
// This-->tab == "structures.h"
#define ETH_MAC_LEN 6

uint8_t broadcast1[3] = {0x01, 0x00, 0x5e};
uint8_t broadcast2[6] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
uint8_t broadcast3[3] = {0x33, 0x33, 0x00};

struct RxControl
{
    signed rssi : 8;
    unsigned rate : 4;
    unsigned is_group : 1;
    unsigned : 1;
    unsigned sig_mode : 2;
    unsigned legacy_length : 12;
    unsigned damatch0 : 1;
    unsigned damatch1 : 1;
    unsigned bssidmatch0 : 1;
    unsigned bssidmatch1 : 1;
    unsigned MCS : 7;
    unsigned CWB : 1;
    unsigned HT_length : 16;
    unsigned Smoothing : 1;
    unsigned Not_Sounding : 1;
    unsigned : 1;
    unsigned Aggregation : 1;
    unsigned STBC : 2;
    unsigned FEC_CODING : 1;
    unsigned SGI : 1;
    unsigned rxend_state : 8;
    unsigned ampdu_cnt : 8;
    unsigned channel : 4;
    unsigned : 12;
};

struct clientinfo
{
    uint8_t station[ETH_MAC_LEN];
    int err;
    signed rssi;
    long lastDiscoveredTime;
};

struct packetBuffer
{
    struct RxControl rx_ctrl;
    uint8_t buf[112];
    uint16_t cnt;
    uint16_t len;
};

struct clientinfo parse_probe(uint8_t *frame, uint16_t framelen, signed rssi)
{
    struct clientinfo pi;
    pi.err = 0;
    pi.rssi = rssi;
    struct packetBuffer *packet = (struct packetBuffer *)frame;
    memcpy(pi.station, frame + 10, ETH_MAC_LEN);
    return pi;
}