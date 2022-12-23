#include <cstdint>

#define OSPF_VERSION 2
#define OSPF_HELLO_INTERVAL 10
#define OSPF_DEAD_INTERVAL 40

#define OSPF_MULTICAST_ADDRESS inet_addr("224.0.0.5")

#define HELLO_OPTION_DC 0b00100000
#define HELLO_OPTION_EA 0b00010000
#define HELLO_OPTION_N  0b00001000
#define HELLO_OPTION_P  0b00001000
#define HELLO_OPTION_MC 0b00000100
#define HELLO_OPTION_E  0b00000010

struct ospf_interface {
    int sock_fd;
    uint32_t ip_address;

    ospf_interface* next;
};

extern ospf_interface* ospf_interface_list;

enum neighbor_state {
    state_down,
    state_init,
    state_2way,
    state_exstart,
    state_exchange,
    state_loading,
    state_full,
};

static const char* neighbor_state_str[] = {
    "Down",
    "Init",
    "2Way",
    "Exstart",
    "Exchange",
    "Loading",
    "Full",
};

struct neighbor {
    uint32_t ip_address;
    neighbor_state state;
    neighbor* next;
};

enum ospf_packet_type {
    hello = 1,
    database_description = 2,
    link_state_request = 3,
    link_state_update = 4,
    link_state_acknoledgement = 4,
};

enum ospf_au_type {
    null_authentication = 0,
    simple_password = 1,
    cryptographic_authentication = 2,
};

enum ls_type {
    router_lsa,
    network_lsa,
    summary_router_lsa,
    summary_network_lsa,
    external_lsa,
};

struct ospf_header {
    uint8_t version;
    uint8_t type;
    uint16_t length;
    uint32_t router_id;
    uint32_t area_id;
    uint16_t checksum;
    uint16_t au_type;
    uint8_t authentication[8];
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
    uint32_t neighbor[];
} __attribute__((packed));

struct ospf_database_description {
    ospf_header header;
    uint16_t interface_mtu;
    uint8_t options;
    uint8_t bits;
    uint32_t dd_seq_num;

} __attribute__((packed));

struct ospf_link_state_request {
    ospf_header header;
    uint32_t ls_type;
    uint32_t link_state_id;
    uint32_t advertising_router[];

} __attribute__((packed));

struct ospf_link_state_update {
    ospf_header header;

} __attribute__((packed));

struct ospf_link_acknowledgement {
    ospf_header header;

} __attribute__((packed));

struct ospf_packet {
    union {
        ospf_header header;
        ospf_hello hello;
        ospf_database_description db_description;
        ospf_link_state_request ls_request;
        ospf_link_state_update ls_update;
        ospf_link_acknowledgement ls_acknowledgement;
    };
} __attribute__((packed));

struct ospf_lsa_header {
    uint16_t age;
    uint8_t options;
    uint8_t lsa_type;
    uint32_t link_state_id;
    uint32_t advertising_router;
    uint32_t sequence_number;
    uint16_t checksum;
    uint16_t kength;
} __attribute__((packed));

extern uint32_t router_id;

void ospf_input(ospf_interface* iface, uint32_t ip_address, uint8_t* buffer, int len);
