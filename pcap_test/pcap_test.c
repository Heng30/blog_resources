#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <pcap.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h> 
#include <net/ethernet.h>
#include <errno.h>

#define MAXBYTES2CAPTURE 2048 
#define MY_PRINTF(format, ...) printf(format"\n", ##__VA_ARGS__)

static void _Process_Packet(u_char *arg, const struct pcap_pkthdr *pkthdr, const u_char *packet) 
{ 
    int i = 0;
	static int counter = 0; 
	struct ether_header *ether_header = (struct ether_header*)packet;
    MY_PRINTF("Packet Count: %d", ++counter); 
    MY_PRINTF("Received Packet Size: %d", pkthdr->len); 
    
	if (ntohs(ether_header->ether_type) == ETHERTYPE_IP) {
		MY_PRINTF("ehter_type: IP");
	} else if (ntohs(ether_header->ether_type) == ETHERTYPE_ARP) {
		MY_PRINTF("ehter_type: ARP");
	} else if (ntohs(ether_header->ether_type) == ETHERTYPE_REVARP) {
		MY_PRINTF("ehter_type: RARP");
	} else {
		MY_PRINTF("ehter_type: %d", ntohs(ether_header->ether_type));
	}
	
	MY_PRINTF("\n************************************************"); 
	MY_PRINTF("HEX:");
    for (i = 0; i < pkthdr->len; i++) { 
        printf("%02x ", (unsigned int)packet[i]); 
        if ( (i % 16 == 15 && i != 0) || (i == pkthdr->len - 1)) { 
            MY_PRINTF(); 
        } 
    } 
	MY_PRINTF("\n************************************************"); 
	MY_PRINTF("plain text:");
	for (i = 0; i < pkthdr->len; i++) { 
		printf("%02c ", packet[i]);
        if ((i % 16 == 15 && i != 0) || (i == pkthdr->len -1)) { 
            MY_PRINTF(); 
        } 
    } 
    MY_PRINTF("\n************************************************"); 
	sleep(1);
} 

int main(int argc, const char *argv[]) 
{ 
	if (argc < 3) {
		MY_PRINTF("Usage: %s interface filter_rule [--saveasfile filepath]", argv[0]);
		exit(EXIT_FAILURE);
	}
	
    int i = 0, count = 0; 
    pcap_t *descr = NULL, *pcap_fp = NULL; 
	pcap_dumper_t *dumper = NULL;
    char errbuf[PCAP_ERRBUF_SIZE] = {0};
	const char *device = NULL; 
    bpf_u_int32 netaddr = 0, mask = 0; 
    struct bpf_program filter; 
	struct in_addr addr, addr_mask;
	
	memset(&filter, 0, sizeof(filter));
	memset (&addr, 0, sizeof(addr));
	 
    device = argv[1]; 

    MY_PRINTF("Try to open device:%s...", device); 
    if(!(descr = pcap_open_live(device, MAXBYTES2CAPTURE, 1, 0, errbuf))) { 
        MY_PRINTF("pcap_open_live error: %s", errbuf); 
        exit(EXIT_FAILURE); 
    } 
	
    if (-1 == pcap_lookupnet(device, &netaddr, &mask, errbuf)) {
		MY_PRINTF("pcap_lookupnet error, errbuf: %s", errbuf);
		exit(EXIT_FAILURE);
	}
	
	addr.s_addr = netaddr;
	addr_mask.s_addr = mask;
	
	MY_PRINTF("open device: %s successfully", device);
	MY_PRINTF("ip: %s", inet_ntoa(addr));
	MY_PRINTF("mask: %s", inet_ntoa(addr_mask));

   if (pcap_compile(descr, &filter, argv[2], 0, mask) < 0) { 
        MY_PRINTF("pcap_compile error: %s", pcap_geterr(descr)); 
        exit(EXIT_FAILURE); 
    } 
	
    if (-1 == pcap_setfilter(descr, &filter)) {
		MY_PRINTF("pcap_sefilter error: %s", pcap_geterr(descr));
		exit(EXIT_FAILURE);
	}
	
	if (argv[3] && !strcmp(argv[3], "--saveasfile") && argv[4]) {
		if (!(dumper = pcap_dump_open(descr, argv[4]))) {
			MY_PRINTF("pcap_dump_open error: %s", pcap_geterr(descr));
			exit(EXIT_FAILURE);
		}
		
		if (-1 == pcap_loop(descr, 0, pcap_dump, (u_char*)dumper)) {
			MY_PRINTF("pcap_loop error: %s", pcap_geterr(descr));
			exit(EXIT_FAILURE);
		}
		
		if (-1 == pcap_dump_flush((pcap_dumper_t*)dumper)) {
			MY_PRINTF("pcap_dump_flush error");
		}
	} else {
		if (-1 == pcap_loop(descr, 0, _Process_Packet, (u_char*)NULL)) {
			MY_PRINTF("pcap_loop error: %s", pcap_geterr(descr));
			exit(EXIT_FAILURE);
		} 
	}
	
	pcap_dump_close(dumper);
	pcap_close(descr);
    exit(EXIT_SUCCESS); 
}
