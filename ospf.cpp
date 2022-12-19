#include "ospf.h"

#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#include <cstdio>
#include <string>

ospf_interface *ospf_interface_list;

neighbor *neighbor_list;

uint16_t checksum_16(uint16_t *buffer, size_t count, uint16_t start) {
    uint32_t sum = start;
    // まず16ビット毎に足す
    while (count > 1) {
        sum += *buffer++;
        count -= 2;
    }
    // もし1バイト余ってたら足す
    if (count > 0) sum += *(uint8_t *)buffer;
    // あふれた桁を折り返して足す
    while (sum >> 16) sum = (sum & 0xffff) + (sum >> 16);

    return ~sum;  // 論理否定(NOT)をとる
}

neighbor *get_neighbor_by_ip(uint32_t ip_address) {
    neighbor *neigh;
    for (neigh = neighbor_list; neigh; neigh = neigh->next) {
        if (neigh->ip_address == ip_address) {
            return neigh;
        }
    }

    return nullptr;
}

void ospf_input(ospf_interface *iface, uint32_t ip_address, uint8_t *buffer,
                int len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", buffer[i]);
    }

    printf("\n");

    ospf_packet *packet = reinterpret_cast<ospf_packet *>(buffer);
    printf("Received ospf packet version %d type %d\n", packet->header.version,
           packet->header.type);

    if (packet->header.version != 2) {
        printf("Unsupported ospf version %d\n", packet->header.version);
        return;
    }

    if (ntohs(packet->header.length) != len) { // OSPFヘッダのlengthと受信したパケット長を比較する
        printf("Invalid ospf packet length\n");
        return;
    }

    neighbor *neigh = get_neighbor_by_ip(ip_address);

    switch (packet->header.type) {
        case ospf_packet_type::HELLO: {

            printf("Received ospf hello packet %d\n", len);

            if(len != 44){
                printf("Hello packet have neighbors\n");
                for(int i=0;i<((len-44)/sizeof(uint32_t));i++){
                    printf("Neighbor %s\n", inet_ntoa(in_addr{.s_addr = packet->hello.neighbor[i]}));
                }
            }

            if (neigh == nullptr) {
                neigh = (neighbor *)calloc(1, sizeof(neighbor));
                neigh->ip_address = ip_address;
                neighbor *next;
                next = neighbor_list;
                neighbor_list = neigh;
                neigh->next = next;
                printf("Registered new neighbor %s\n",
                       inet_ntoa(in_addr{.s_addr = ip_address}));
            }

            ospf_hello hello_rep;
            hello_rep.header.version = 2;
            hello_rep.header.type = ospf_packet_type::HELLO;
            hello_rep.header.length = htons(48);
            hello_rep.header.router_id = inet_addr("5.5.5.5");
            hello_rep.header.area_id = htonl(0);
            hello_rep.header.checksum = htons(0);
            hello_rep.header.au_type = 0;
            hello_rep.header.authentication = 0;

            hello_rep.network_mask = inet_addr("255.255.255.0");
            hello_rep.hello_interval = htons(10);
            hello_rep.option = 0x02; // External routing
            hello_rep.router_priority = 1;
            hello_rep.dead_interval = htonl(40);
            hello_rep.designed_router = inet_addr("5.5.5.5");
            hello_rep.backup_designed_router = 0;
            hello_rep.neighbor[0] = ip_address;

                    hello_rep.header.checksum =
                checksum_16(reinterpret_cast<uint16_t *>(&hello_rep), 48, 0);

            sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = inet_addr("224.0.0.5");

            printf("Sending hello packet\n");

            if(sendto(iface->sock_fd, &hello_rep, 48, 0, (sockaddr *)&addr, sizeof(sockaddr_in)) < 0){
                perror("Failed to send");
                return;
            }
        }

            break;
        case ospf_packet_type::DATABASE_DESCRIPTION:
            if (neigh == nullptr) {
                printf("Received database description from unknown host\n");
                return;
            }
            break;
        case ospf_packet_type::LINK_STATE_REQUEST:
            if (neigh == nullptr) {
                printf("Received link state request from unknown host\n");
                return;
            }
    
            break;
        default:
            printf("Unsupported packet type %d\n", packet->header.type);
            return;
    }
}