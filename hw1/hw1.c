#include <sys/types.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include "pcap.h"
#include "ip_headers.h"

#define READ_BUFFER_SIZE 8192
#define IP_BUFFER_SIZE 32

int swapped;

/* swapping order according to information */
int swapInt(int in)
{
	if(swapped)
	{
		int out;
		char* ci = (char *) &in;
		char* co = (char *) &out;
		co[0] = ci[3];
		co[1] = ci[2];
		co[2] = ci[1];
		co[3] = ci[0];
		return out;
	}
	return in;
}

short swapShort(short in)
{
	if(swapped)
	{
		short out;
		char* ci = (char *) &in;
		char* co = (char *) &out;
		co[0] = ci[1];
		co[1] = ci[0];
		return out;
	}
	return in;
}

/* get a header */
unsigned fetchGlobalHeader(int fp, pcap_hdr_t* globalHeader)
{
	int result;
	result = read (fp, globalHeader, sizeof(pcap_hdr_t));

	swapped = 0;
	if (globalHeader->magic_number == 0xd4c3b2a1)
	{
		swapped = 1;
	}
	else if (globalHeader->magic_number != 0xa1b2c3d4)
	{
		return 0;
	}

	return result;
}


unsigned fetchRecordHeader(int fp, pcaprec_hdr_t* recordHeader)
{
	return read(fp, recordHeader, sizeof(pcaprec_hdr_t));
}





int main(int argc, const char *argv[])
{
	int fp;
	char globalHeaderContent[sizeof(pcap_hdr_t)];
	char recordHeaderContent[sizeof(pcaprec_hdr_t)];
	pcap_hdr_t* globalHeader = (pcap_hdr_t*) globalHeaderContent;
	pcaprec_hdr_t* recordHeader = (pcaprec_hdr_t*) recordHeaderContent;
	unsigned int recordCount = 0;
	char buffer[READ_BUFFER_SIZE];
	char ipCharacterBuffer[IP_BUFFER_SIZE];

	ether_header_t* etherHeader;
	ipv4_header_t* ipv4Header;
	udp_header_t* udpHeader;

	if (argc<1) 
	{
		fprintf(stderr, "Error: No file input.\n");
		return 1;
	}
	

	/* file opening */
	fp = open(argv[1], O_RDONLY);
	if (fp == -1)
	{
		fprintf(stderr, "Error: Cannot open file.\n");
		return 2;
	}


	if (!fetchGlobalHeader(fp, globalHeader))
	{
		fprintf(stderr, "Error: File format not correct\n");
		return 4;
	}

	



	printf ("ver=%d.%d snaplen=%d network=%d\n",
		swapShort(globalHeader->version_major),
		swapShort(globalHeader->version_minor),
		swapInt(globalHeader->snaplen),
		swapInt(globalHeader->network));




	while(fetchRecordHeader(fp, recordHeader))
	{
		recordCount+=1;

		read (fp, buffer, swapInt(recordHeader->incl_len));

		etherHeader=(ether_header_t *) (buffer);

		ipv4Header=(ipv4_header_t *) (buffer
				+sizeof(ether_header_t));



		printf ("%u.%06u %u/%u %s -> %s (%u)", 
				swapInt(recordHeader->ts_sec),
				swapInt(recordHeader->ts_usec),
				swapInt(recordHeader->incl_len),
				swapInt(recordHeader->orig_len),
				inet_ntoa (ipv4Header->ip_src),
				inet_ntoa (ipv4Header->ip_dst),
				ipv4Header->ip_p
			);



		if (ipv4Header->ip_p == 17)
		{
			udpHeader=(udp_header_t *) (buffer
					+sizeof(ether_header_t)
					+sizeof(ipv4_header_t));

			printf(" sport=%u dport=%u",
					ntohs(udpHeader->port_src),
					ntohs(udpHeader->port_dst)
			      );
		}

		printf("\n");
	}

	printf ("total %d packets read\n", recordCount);


	close(fp);

	return 0;
}
