#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#include <pcre.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>

#define OPTBUFFER 1024
#define DATABUFFER 524288

bool parseUrl (char *url, char *domainName, unsigned short *port, char *path)
{
	/* use PCRE regular expression */
	const char *error;
	int erroffset, posvec[15], re;
	const char **listptr;
	pcre *regexp;

	regexp = pcre_compile ("^http://([\\w\\d\\.\\-]+)(\\:\\d+)?(.*)$", 0, &error, &erroffset, NULL);
	if (!regexp)
	{
		/*THIS SHOULD NOT HAPPEN AT ALL!!! */
		printf ("Regular Expression compilation failed.\n");
		return 1;
	}

	re = pcre_exec (regexp, NULL, url, strlen(url), 0, PCRE_PARTIAL, posvec, 15);
	if (re < 0)
	{
		printf ("Not a good HTTP URL\n");
		return false;
	}

	pcre_get_substring_list (url, posvec, re, &listptr);

	strncpy (domainName, listptr[1], OPTBUFFER);
	strncpy (path, listptr[3], OPTBUFFER);
	sscanf (listptr[2], ":%d", port);

	if (*port == 0)
	{
		*port = 80;
	}

	if (strcmp ("", path) == 0)
	{
		strncpy (path, "/", OPTBUFFER);
	}

	/* free pcre string finally */
	pcre_free_substring_list (listptr);
}



int sendHttpRequest(char *url, char *buffer)
{
	unsigned short port;
	char domainName[OPTBUFFER], path[OPTBUFFER];
	int sockfd;
	struct hostent *hostInfo;
	struct sockaddr_in addr;
	int len=0, clen;
	char sendBuffer[OPTBUFFER];

	parseUrl (url, domainName, &port, path);

	sprintf (sendBuffer, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, domainName);

	memset(&addr, 0, sizeof(addr));
	hostInfo = gethostbyname(domainName);
	addr.sin_family = AF_INET;
	memcpy(&(addr.sin_addr), hostInfo->h_addr, hostInfo->h_length);
	addr.sin_port = htons(port);

	sockfd = socket (AF_INET, SOCK_STREAM, 0);
	connect (sockfd, (struct sockaddr *) &addr, sizeof(struct sockaddr));

	send (sockfd, sendBuffer, strlen(sendBuffer), 0);

	do
	{
		clen = recv(sockfd, buffer+len, DATABUFFER - len, 0);
		len += clen;
	}
	while (clen > 0);

	buffer[len] = '\0';

	close(sockfd);

	return len;
}



int main (int argc, char **argv)
{
	char *url; 
	bool disableHeader = false;

	char buffer[DATABUFFER];



	/* use getopt */
	int opt;
	char *optString = "x";

	while ((opt = getopt(argc, argv, optString)) != -1)
	{
		if (opt == 'x')
		{
			disableHeader = true;
		}
	}



	
	/* Get the last option */
	url = *(argv + optind);



	sendHttpRequest(url, buffer);



	/* print result */
	if (disableHeader)
	{
		/* skip header */
		puts(strstr(buffer, "\r\n\r\n")+4);
	}
	else
	{
		puts(buffer);
	}




	return 0;
}
