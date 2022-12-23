#include <arpa/inet.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>

#include <cmath>
#include <csignal>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>

#include "ospf.h"

#define IPPROTO_OSPF 0x59

int main() {
    printf("Hello OSPF!\n");

    /** AF_INETでMULTICASTフラグが有効なインターフェース毎にsocketを作成し、マルチキャストグループにJOINする **/
    ifaddrs *addrs;
    ip_mreq mreq;

    getifaddrs(&addrs);  // 全てのインターフェースの情報を取得

    for (ifaddrs *tmp = addrs; tmp; tmp = tmp->ifa_next) {
        if (tmp->ifa_addr && tmp->ifa_addr->sa_family == AF_INET &&
            (tmp->ifa_flags & IFF_MULTICAST)) {
            // 各インターフェースをOSPFインターフェースという構造体で管理する
            ospf_interface *iface;         
            iface = (ospf_interface *)calloc(1, sizeof(ospf_interface));

            ospf_interface *next;
            next = ospf_interface_list;
            ospf_interface_list = iface;
            iface->next = next;

            // socketの作成
            iface->sock_fd = socket(AF_INET, SOCK_RAW, IPPROTO_OSPF);

            if (iface->sock_fd < 0) {
                perror("Failed to create socket");
                return 1;
            }
            iface->ip_address = ((sockaddr_in *)tmp->ifa_addr)->sin_addr.s_addr;

            // socketをネットワークインターフェースにバインド
            if (setsockopt(iface->sock_fd, SOL_SOCKET, SO_BINDTODEVICE,
                           tmp->ifa_name, strlen(tmp->ifa_name) + 1) < 0) {
                perror("Failed to setsockopt");
                return 1;
            }

            // TODO IP_HDRINCLソケットオプションが無効になっていることを確認する

            // マルチキャストグループに参加
            memset(&mreq, 0, sizeof(mreq));
            mreq.imr_interface.s_addr =
                ((sockaddr_in *)tmp->ifa_addr)->sin_addr.s_addr;
            mreq.imr_multiaddr.s_addr = inet_addr("224.0.0.5");

            if (setsockopt(iface->sock_fd, IPPROTO_IP, IP_ADD_MEMBERSHIP,
                           (char *)&mreq, sizeof(mreq)) != 0) {
                perror("Failed to setsockopt");
                return 1;
            }

            printf("Joined multicast group %s %s\n", tmp->ifa_name,
                   inet_ntoa(((sockaddr_in *)tmp->ifa_addr)->sin_addr));
        }
    }

    freeifaddrs(addrs);

    /** ファイルディスクリプタ監視用のディスクリプタを作成する **/
    fd_set sets, read_sets;
    //timeval tv;
    //tv.tv_sec = 1;
    //tv.tv_usec = 0;
    int max_fd = 0;
    FD_ZERO(&sets);

    ospf_interface *iface;
    for (iface = ospf_interface_list; iface; iface = iface->next) {
        max_fd = std::max(max_fd, iface->sock_fd);
        FD_SET(iface->sock_fd, &sets);
    }

    router_id = inet_addr("250.0.0.250");

    /** パケット処理をする **/
    while (true) {
        memcpy(&read_sets, &sets, sizeof(fd_set));

        printf("Selecting...\n");

        int rc = select(max_fd + 1, &read_sets, nullptr, nullptr, nullptr);

        printf("Select exited!\n");

        uint8_t buffer[2000];
        int n;
        sockaddr_in addr;
        socklen_t addr_len;

        for (iface = ospf_interface_list; iface; iface = iface->next) {
            if (FD_ISSET(iface->sock_fd, &read_sets)) {
                n = recvfrom(iface->sock_fd, buffer, sizeof(buffer), 0,
                             (sockaddr *)&addr, &addr_len);
                if (n < 0) {
                    perror("Failed to receive");
                    return 1;
                }
                if (addr_len != sizeof(sockaddr_in)) {
                    printf("Invalid socket address length %d\n", addr_len);
                    continue;
                }
                if (addr.sin_addr.s_addr == iface->ip_address) {
                    continue;
                }
                printf("%d bytes received from %s!\n", n,
                       inet_ntoa(addr.sin_addr));

                uint32_t hlen = (reinterpret_cast<iphdr *>(buffer))->ihl << 2;

                if (hlen > n) {
                    printf("Illigal ip header size\n");
                    continue;
                }

                ospf_input(iface, addr.sin_addr.s_addr, buffer + hlen, n - hlen);

                FD_CLR(iface->sock_fd, &read_sets);
            }
        }
    }

    return 0;
}
