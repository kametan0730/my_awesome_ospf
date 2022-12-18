#include "ospf.h"

#include <cstdio>

ospf_interface* ospf_interface_list;

void ospf_input(uint8_t* buffer, int len){

    for (size_t i = 0; i < len; i++)
    {
        printf("%02x", buffer[i]);
    }

    printf("\n");
    

    ospf_header* hdr = reinterpret_cast<ospf_header*>(buffer);
    printf("version %d type %d\n", hdr->version, hdr->type);

}