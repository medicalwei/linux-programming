typedef struct ether_header_s {
	unsigned char ether_dhost[6];
	unsigned char ether_shost[6];
	unsigned short ether_type;
} __attribute__ ((__packed__)) ether_header_t;

typedef struct ipv4addr_s{
	unsigned char addr[4];
} __attribute__ ((__packed__)) ipv4addr_t;

/* from Wikipedia: http://en.wikipedia.org/wiki/Ip_(struct) */
typedef struct ipv4_header_s {
	unsigned int ip_hl:4; /* both fields are 4 bits */
	unsigned int ip_v:4;
	unsigned char ip_tos;
	unsigned short ip_len;
	unsigned short ip_id;
	unsigned short ip_off;
	unsigned char ip_ttl;
	unsigned char ip_p;
	unsigned short ip_sum;
	ipv4addr_t ip_src;
	ipv4addr_t ip_dst;
} __attribute__ ((__packed__)) ipv4_header_t;

typedef struct udp_header_s {
	unsigned short port_src;
	unsigned short port_dst;
	unsigned short length;
	unsigned short checksum;
	unsigned int data;
} __attribute__ ((__packed__)) udp_header_t;

