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

/* options buffer size */
#define OPTBUFFER 1024

/* return buffer size */
#define DATABUFFER 524288




bool parseUrl (char *url, char *domainName, unsigned short *port, char *path)
{
	/* use PCRE regular expression */
	const char *error;
	int erroffset, posvec[15], re;
	const char **listptr;
	pcre *regexp;


	*port = 0;

	
	/* compiling RE */
	regexp = pcre_compile ("^http://([\\w\\d\\.\\-]+)(\\:\\d+)?(.*)$", 0, &error, &erroffset, NULL);
	if (!regexp)
	{
		/*THIS SHOULD NOT HAPPEN AT ALL!!! */
		printf ("Regular Expression compilation failed.\n");
		return false;
	}



	/* matching RE */
	re = pcre_exec (regexp, NULL, url, strlen(url), 0, PCRE_PARTIAL, posvec, 15);
	if (re < 0)
	{
		printf ("Not a good HTTP URL\n");
		return false;
	}





	/* get substring and parse */
	pcre_get_substring_list (url, posvec, re, &listptr);

	strncpy (domainName, listptr[1], OPTBUFFER);
	strncpy (path, listptr[3], OPTBUFFER);
	sscanf (listptr[2], ":%d", port);



	/* define options if these are missing */
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

	return true;
}



int sendHttpRequest (char *url, char *buffer)
{
	/* used for port number, domain name and path */
	unsigned short port;
	char domainName[OPTBUFFER], path[OPTBUFFER];

	/* used for sock descriptor */
	int sockfd;

	/* used to send IP */
	struct hostent *hostInfo;
	struct sockaddr_in addr;

	/* used to count length */
	int len=0, totalLen=0;

	/* used to assemble message sending to the host */
	char sendBuffer[OPTBUFFER];





	/* parse URL */
	if (!parseUrl (url, domainName, &port, path))
	{
		/* error occurred while parsing URL */
		exit(1);
	}



	/* look up the domain name, and then put host, port number into addr */
	memset (&addr, 0, sizeof (addr));
	hostInfo = gethostbyname (domainName);

	addr.sin_family = AF_INET;
	memcpy (&(addr.sin_addr), hostInfo->h_addr, hostInfo->h_length);
	addr.sin_port = htons (port);



	/* prepare for sending message */
	sprintf (sendBuffer, "GET %s HTTP/1.0\r\nHost: %s\r\n\r\n", path, domainName);



	/* create a socket, connect to the host then send message */
	sockfd = socket (AF_INET, SOCK_STREAM, 0);
	connect (sockfd, (struct sockaddr *) &addr, sizeof (struct sockaddr));

	send (sockfd, sendBuffer, strlen(sendBuffer), 0);



	/* collect received message into buffer, until full or message ends */
	do
	{
		len = recv (sockfd, buffer + totalLen, DATABUFFER - totalLen, 0);
		totalLen += len;
	}
	while (len > 0);



	/* null out the last one to prevent errors */
	buffer[totalLen] = '\0';



	/* closing socket */
	close (sockfd);



	return totalLen;
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



	/* send HTTP request */
	sendHttpRequest (url, buffer);



	/* print result */
	if (disableHeader)
	{
		/* skip header */
		puts (strstr (buffer, "\r\n\r\n")+4);
	}
	else
	{
		puts (buffer);
	}




	return 0;
}
