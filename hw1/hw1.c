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

/* global variable to ensure if the byte order needs to swap */
int swapped;




/* swapping order according to information */
int swapInt (int in)
{
	if (swapped)
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

short swapShort (short in)
{
	if (swapped)
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





/* get a global header */
unsigned fetchGlobalHeader (int fp, pcap_hdr_t* globalHeader)
{
	int result;
	result = read (fp, globalHeader, sizeof(pcap_hdr_t));

	swapped = 0;
	if (globalHeader->magic_number == 0xd4c3b2a1)
	{
		/* swapping byte order for functions above
		 * (I have an annoying PowerPC laptop >w<) */
		swapped = 1;
	}
	else if (globalHeader->magic_number != 0xa1b2c3d4)
	{
		return 0;
	}

	return result;
}

/* get a record header */
unsigned fetchRecordHeader (int fp, pcaprec_hdr_t* recordHeader)
{
	return read(fp, recordHeader, sizeof(pcaprec_hdr_t));
}








int main (int argc, const char *argv[])
{
	/* file buffers */
	int fp;
	char globalHeaderContent[sizeof(pcap_hdr_t)];
	char recordHeaderContent[sizeof(pcaprec_hdr_t)];
	char buffer[READ_BUFFER_SIZE];

	/* headers, headers, headers... */
	pcap_hdr_t* globalHeader = (pcap_hdr_t*) globalHeaderContent;
	pcaprec_hdr_t* recordHeader = (pcaprec_hdr_t*) recordHeaderContent;
	ether_header_t* etherHeader;
	ipv4_header_t* ipv4Header;
	udp_header_t* udpHeader;

	/* record count */
	unsigned int recordCount = 0;





	/* checking file input */
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





	/* applying global header data structure */
	if (!fetchGlobalHeader(fp, globalHeader))
	{
		fprintf(stderr, "Error: File format not correct\n");
		return 4;
	}

	/* printing global header information */
	printf ("ver=%d.%d snaplen=%d network=%d\n",
		swapShort(globalHeader->version_major),
		swapShort(globalHeader->version_minor),
		swapInt(globalHeader->snaplen),
		swapInt(globalHeader->network));





	/* per-packet information */
	while(fetchRecordHeader(fp, recordHeader))
	{
		recordCount += 1;

		/* read file */
		read (fp, buffer, swapInt(recordHeader->incl_len));


		/* applying data structures */
		etherHeader = (ether_header_t *) (buffer);
		ipv4Header = (ipv4_header_t *) (buffer
				+sizeof(ether_header_t));



		/* printing
		 * 1234567890.098765 100/100 123.45.67.89 -> 98.76.54.32 (17) sport=12345 dport=9876 */
		printf ("%u.%06u %u/%u", 
				swapInt (recordHeader->ts_sec),
				swapInt (recordHeader->ts_usec),
				swapInt (recordHeader->incl_len),
				swapInt (recordHeader->orig_len),
			);
		printf (" %s", inet_ntoa (ipv4Header->ip_src));
		printf (" -> %s", inet_ntoa (ipv4Header->ip_dst));
		printf (" (%u)", ipv4Header->ip_p);



		/* UDP-specific information */
		if (ipv4Header->ip_p == 17)
		{

			/* applying UDP header data structure */
			udpHeader = (udp_header_t *) (buffer
					+ sizeof (ether_header_t)
					+ sizeof (ipv4_header_t));

			/* printing */
			printf(" sport=%u dport=%u",
					ntohs (udpHeader->port_src),
					ntohs (udpHeader->port_dst)
			      );

		}


		/* new line */
		printf("\n");
	}

	printf ("total %d packets read\n", recordCount);







	close(fp);

	return 0;
}
