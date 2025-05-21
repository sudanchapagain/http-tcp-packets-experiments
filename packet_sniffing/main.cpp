#include <iostream>
#include <pcap.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>

/*

pcap.h (libpcap) provides functions for capturing network packets
ip.h for ip header structure
if_ether.h for ethernet header structure

*/

void packetHandler(u_char *userData, const struct pcap_pkthdr *pkthdr, const u_char *packet) {
    // cast the raw packet to an Ethernet header (ethhdr)
    // move it the IP header (iphdr) by skipping the size of the Ethernet header
    const struct ethhdr *eth = (struct ethhdr *) packet;
    const struct iphdr *ip = (struct iphdr *)(packet + sizeof(struct ethhdr));

    std::cout << "captured packet:" << std::endl;
    std::cout << "source ip: " << static_cast<int>(ip->saddr & 0xFF) << "."
              << static_cast<int>((ip->saddr >> 8) & 0xFF) << "."
              << static_cast<int>((ip->saddr >> 16) & 0xFF) << "."
              << static_cast<int>((ip->saddr >> 24) & 0xFF) << std::endl;
    std::cout << "destination ip: " << static_cast<int>(ip->daddr & 0xFF) << "."
              << static_cast<int>((ip->daddr >> 8) & 0xFF) << "."
              << static_cast<int>((ip->daddr >> 16) & 0xFF) << "."
              << static_cast<int>((ip->daddr >> 24) & 0xFF) << std::endl;
    std::cout << "packet length: " << pkthdr->len << " bytes" << std::endl;
    std::cout << "-----------------------------------" << std::endl;
}

int main() {
    // pcap_findalldevs to list all available netwoek interfaces
    // alldevs selects first available
    // pcap_open_live to open the device
    char errbuf[256];
    pcap_if_t *alldevs;

    if (pcap_findalldevs(&alldevs, errbuf) == -1) {
        std::cerr << "error finding devices: " << errbuf << std::endl;
        return 1;
    }

    const pcap_if_t *device = alldevs;
    if (device == nullptr) {
        std::cerr << "no devices found." << std::endl;
        return 1;
    }

    std::cout << "using device: " << device->name << std::endl;
    pcap_t *handle = pcap_open_live(device->name, BUFSIZ, 1, 1000, errbuf);
    if (handle == nullptr) {
        std::cerr << "couldn't open device: " << errbuf << std::endl;
        return 1;
    }

    // pass the callback packetHandlr
    pcap_loop(handle, 10, packetHandler, nullptr);

    pcap_close(handle);
    pcap_freealldevs(alldevs);
    return 0;
}
