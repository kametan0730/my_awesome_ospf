#include <cstdint>

struct ospf_interface {
    int sock_fd;
    uint32_t ip_address;

    ospf_interface* next;
};

extern ospf_interface* ospf_interface_list;

struct neighbor {
    uint32_t ip_address;

    neighbor* next;
};

enum ospf_packet_type {
    HELLO = 1,
    DATABASE_DESCRIPTION = 2,
    LINK_STATE_REQUEST = 3,
    LINK_STATE_UPDATE = 4,
    LINK_STATE_ACKNOLEDGEMENT = 4,
};

struct ospf_header {
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t router_id;
    uint32_t area_id;
    uint16_t checksum;
    uint16_t au_type;
    uint64_t authentication;
} __attribute__((packed));

struct ospf_hello {
    ospf_header header;
    uint32_t network_mask;
    uint16_t hello_interval;
    uint8_t option;
    uint8_t router_priority;
    uint32_t dead_interval;
    uint32_t designed_router;
    uint32_t backup_designed_router;
    uint32_t neighbor[64];
} __attribute__((packed));

struct ospf_packet {
    union {
        ospf_header header;
        ospf_hello hello;
    };
} __attribute__((packed));

void ospf_input(ospf_interface* iface, uint32_t ip_address, uint8_t* buffer, int len);
