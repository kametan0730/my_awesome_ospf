#include "ospf.h"

#include <cstdio>
#include <cstring>
#include <string>
#include <arpa/inet.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>

ospf_interface *ospf_interface_list;

neighbor *neighbor_list;

uint32_t router_id;

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

void change_neighbor_state(neighbor* neigh, neighbor_state new_state){
    printf("Neighbor state changed %s: %s => %s\n",
           inet_ntoa(in_addr{.s_addr = neigh->ip_address}),
           neighbor_state_str[neigh->state], neighbor_state_str[new_state]);
    neigh->state = new_state;
}

void ospf_input(ospf_interface *iface, uint32_t ip_address, uint8_t *buffer, int len) {
    for (size_t i = 0; i < len; i++) {
        printf("%02x", buffer[i]);
    }

    printf("\n");

    ospf_packet *packet = reinterpret_cast<ospf_packet *>(buffer);
    printf("Received ospf packet version %d\n", packet->header.version);

    if (packet->header.version != OSPF_VERSION) {
        printf("Unsupported ospf version %d\n", packet->header.version);
        return;
    }

    if (ntohs(packet->header.length) != len) { // OSPFヘッダのlengthと受信したパケット長を比較する
        printf("Invalid ospf packet length\n");
        return;
    }

    neighbor *neigh = get_neighbor_by_ip(ip_address);

    switch (packet->header.type) {
        case ospf_packet_type::hello: {

            if (len < sizeof(ospf_hello)) {
                printf("Invalid hello packet length %d\n", len);
                return;
            }

            printf("Received ospf hello packet %d\n", len);

            if (neigh == nullptr) {
                neigh = (neighbor *)calloc(1, sizeof(neighbor));
                neigh->ip_address = ip_address;
                neighbor *next;
                next = neighbor_list;
                neighbor_list = neigh;
                neigh->next = next;

                neigh->state = neighbor_state::state_down;
                change_neighbor_state(neigh, neighbor_state::state_init);

                printf("Registered new neighbor %s\n",
                       inet_ntoa(in_addr{.s_addr = ip_address}));
            }else{
                if(neigh->state == neighbor_state::state_down){
                    change_neighbor_state(neigh, neighbor_state::state_init);

                    printf("Initialized neighbor %s\n",
                           inet_ntoa(in_addr{.s_addr = ip_address}));
                }
            }

            if (len != sizeof(ospf_hello)) {  // neighborを含むとき
                for (int i = 0; i < ((len - sizeof(ospf_hello)) / sizeof(uint32_t)); i++) {
                    printf("Hello packet has neighbor %s\n",inet_ntoa(in_addr{.s_addr = packet->hello.neighbor[i]}));

                    if (packet->hello.neighbor[i] == router_id) {
                        if(neigh->state == neighbor_state::state_init){
                            printf("This neighbor!\n");
                            change_neighbor_state(neigh, neighbor_state::state_2way);
                        }
                    }
                }
            }

            uint8_t rep_buffer[1000];
            memset(rep_buffer, 0x00, 1000);
            ospf_hello* hello_rep;
            uint32_t neighbor_count = 0;
            hello_rep = reinterpret_cast<ospf_hello*>(rep_buffer);

            hello_rep->header.version = OSPF_VERSION;
            hello_rep->header.type = ospf_packet_type::hello;
            hello_rep->header.length = 0;
            hello_rep->header.router_id = router_id; // neighborより小さいと先にDBDを送ってくれない
            hello_rep->header.area_id = inet_addr("0.0.0.0");  // バックボーンエリア
            hello_rep->header.checksum = 0;
            hello_rep->header.au_type = ospf_au_type::null_authentication;

            memset(hello_rep->header.authentication, 0x00,
                   sizeof(hello_rep->header.authentication));

            hello_rep->network_mask = inet_addr("255.255.255.0");
            hello_rep->hello_interval = htons(OSPF_HELLO_INTERVAL);
            hello_rep->option = (HELLO_OPTION_E); // External routing
            hello_rep->router_priority = 1;
            hello_rep->dead_interval = htonl(OSPF_DEAD_INTERVAL);
            hello_rep->designed_router = packet->hello.designed_router;
            hello_rep->backup_designed_router = 0;
            hello_rep->neighbor[0] = ip_address;
            neighbor_count++;

            uint16_t packet_len = sizeof(ospf_hello) + neighbor_count * sizeof(uint32_t);

            hello_rep->header.length = htons(packet_len);

            hello_rep->header.checksum = checksum_16(
                reinterpret_cast<uint16_t *>(&hello_rep), packet_len, 0);

            sockaddr_in addr;
            addr.sin_family = AF_INET;
            addr.sin_addr.s_addr = OSPF_MULTICAST_ADDRESS;

            printf("Sending hello packet\n");

            if (sendto(iface->sock_fd, rep_buffer, packet_len, 0,
                       (sockaddr *)&addr, sizeof(sockaddr_in)) < 0) {
                perror("Failed to send");
                return;
            }
        }

            break;
        case ospf_packet_type::database_description:
            if (neigh == nullptr) {
                printf("Received database description from unknown host\n");
                return;
            }
            printf("Received db description mtu %d\n", htons(packet->db_description.interface_mtu));

            break;
        case ospf_packet_type::link_state_request:
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