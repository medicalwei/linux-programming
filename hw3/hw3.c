/* mostly from Fred's blog http://fred-zone.blogspot.com/2007/09/http-web-server.html */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include <dirent.h>

#define BUFSIZE 8192

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },
	{"jpg", "image/jpeg"},
	{"jpeg","image/jpeg"},
	{"png", "image/png" },
	{"zip", "image/zip" },
	{"gz",  "image/gz"  },
	{"tar", "image/tar" },
	{"htm", "text/html" },
	{"html","text/html" },
	{"css", "text/css" },
	{"js",  "text/javascript" },
	{0,0} };

char siteUri[] = "http://localhost";
int portNumber;

char listContainerHeader[] = 
"HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"	<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">\n"
"	<title>Index of %s</title>\n"
"	<style>html{font-family:\"Helvetica Neue\",\"Helvetica\",\"Nimbus Sans\",\"Arial\",\"微軟正黑體\",\"Microsoft JhengHei\",\"LiHei Pro\",sans-serif;color:#666;background:url(\"artworks/ntou.png\") 95\% bottom no-repeat #ccc;min-height:100\%;}body{width:720px;margin:0 auto;padding:0 0 40pt;font-weight:300;}h1{font-weight:500;text-shadow:0 2px 0 rgba(0,0,0,0.2),0 -1px 0 rgba(255,255,255,0.2);color:#666;margin-top:40pt;}a{color:#333;border-bottom:1px solid #ccc;text-decoration:none;-webkit-transition:border-color 0.3s linear;-moz-transition:border-color 0.3s linear;-o-transition:border-color 0.3s linear;transition:border-color 0.3s linear;}a:hover{border-bottom:1px solid #999;-webkit-transition:border-color 0.1s linear;-moz-transition:border-color 0.1s linear;-o-transition:border-color 0.1s linear;transition:border-color 0.1s linear;}</style>\n"
"</head>\n"
"<body>\n"
"	<h1>Index of %s</h1>\n"
"	<ul id=\"listing\">\n";

char listContainerContent[] =
"		<li><a href=\"%s:%d%s%s\">%s</a></li>\n";

char listContainerFooter[] =
"	</ul>\n"
"</body>\n"
"</html>\n";

#define WRITE_OK 16
#define FORBIDDEN 8
#define FILE_NOT_SUPPORTED 4
#define NOT_EXIST 2
#define IS_DIR 1
#define IS_FILE 0

int outputListing (int fd, char *directoryBuffer, char *outputBuffer)
{
	/* partially from http://www.gnu.org/s/hello/manual/libc/Simple-Directory-Lister.html */
	char directoryName[BUFSIZE+1];
	
	DIR *dp;
	struct dirent *ep;
	
	if(directoryBuffer[0] == 0)
	{
		dp = opendir (".");
	}
	else
	{
		dp = opendir (directoryBuffer);
	}

	if (dp == NULL)
	{
		sprintf(outputBuffer, "HTTP/1.1 403 Forbidden\r\n\r\n");
		write(fd, outputBuffer, strlen(outputBuffer));
		return FORBIDDEN;
	}

	sprintf(directoryName, "/%s", directoryBuffer);
	sprintf(outputBuffer, listContainerHeader, directoryName, directoryName);
	write(fd, outputBuffer, strlen(outputBuffer));

	while (ep = readdir (dp))
	{
		if (ep->d_name[0] == '.')
		{
			continue;
		}
		sprintf(outputBuffer, listContainerContent,
			 siteUri,
			 portNumber,
			 directoryName,
			 ep->d_name,
			 ep->d_name
			);
		write(fd, outputBuffer, strlen(outputBuffer));
	}
	(void) closedir (dp);

	sprintf(outputBuffer, listContainerFooter);
	write(fd, outputBuffer, strlen(outputBuffer));
	
	return WRITE_OK;
}

int writeFile (int fd, char *uriBuffer, char *outputBuffer)
{
	int buflen = 0;
	char* fstr = NULL;
	int i, ret, len, file_fd;

	buflen = strlen(uriBuffer);

	/* check filetype */
	for(i=0; extensions[i].ext!=0; i++) {
		len = strlen(extensions[i].ext);
		if(!strncmp(&uriBuffer[buflen-len], extensions[i].ext, len)) {
			fstr = extensions[i].filetype;
			break;
		}
	}

	/* open file */
	if (!(file_fd=open(uriBuffer, O_RDONLY))) {
		sprintf(outputBuffer,"HTTP/1.1 403 Forbidden\r\n\r\n");
		write(fd, outputBuffer, strlen(outputBuffer));
		return FORBIDDEN;
	}

	/* file unsupported */
	if(fstr == 0) {
		/* return FILE_NOT_SUPPORTED; */
		sprintf(outputBuffer,"HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n", fstr);
	}
	else
	{
		/* return 200 then its content */
		sprintf(outputBuffer,"HTTP/1.1 200 OK\r\nContent-Type: %s\r\n\r\n", fstr);
	}

	write(fd, outputBuffer, strlen(outputBuffer));

	while ((ret=read(file_fd, outputBuffer, BUFSIZE))>0) {
		write(fd, outputBuffer, ret);
	}

	return WRITE_OK;
}

/* check if file is dir */
int checkFileStatus(const char *dname) {
	struct stat sbuf;

	if (lstat(dname, &sbuf) == -1)
	{
		return NOT_EXIST;
	}
 
	if(S_ISDIR(sbuf.st_mode))
	{
		return IS_DIR;
	}
 
	return IS_FILE;
}


void handle_socket(int fd)
{
	int file_fd, urilen;
	long i, ret;
	char * fstr;
	static char cmdBuffer[BUFSIZE+1], uriBuffer2[BUFSIZE+1];
	static char outputBuffer[BUFSIZE+1];
	static char *uriBuffer;

	ret = read(fd,cmdBuffer,BUFSIZE);   /* get browser config */
	if (ret==0||ret==-1) {
	 /* connection broken */
		exit(3);
	}

	/* insert null character for the end of the string */
	if (ret>0&&ret<BUFSIZE)
		cmdBuffer[ret] = 0;
	else
		cmdBuffer[0] = 0;

	/* remove return */
	for (i=0;i<ret;i++) 
	{
		if (cmdBuffer[i]=='\r' || cmdBuffer[i]=='\n')
		{
			cmdBuffer[i] = 0;
		}
	}
	
	/* TODO: answer POST requests when it is CGI */
	/* only receive GET */
	if (strncmp(cmdBuffer,"GET ",4)&&strncmp(cmdBuffer,"get ",4))
	{
		exit(3);
	}
	

	/* use null character */
	for(i=4;i<BUFSIZE;i++) {
		if(cmdBuffer[i] == ' ') {
			cmdBuffer[i] = 0;
			break;
		}
	}

	/* remove parent directory to prevent insecurity */
	for (i=0;i<ret-1;i++)
	{
		if (cmdBuffer[i]=='.' && cmdBuffer[i+1]=='.')
		{
			exit(3);
		}
	}

	/* get URI string */
	uriBuffer = &cmdBuffer[5];
	urilen = strlen(uriBuffer);

	if (urilen == 0)
	{
		sprintf(uriBuffer2, "index.html", uriBuffer);

		switch (checkFileStatus(uriBuffer2)) {
			case NOT_EXIST:
			case IS_DIR:
				outputListing(fd, "", outputBuffer);
				/* directory generation */
				break;
			default:
				writeFile(fd, uriBuffer2, outputBuffer);
				break;
		}

		exit(1);
	}

	switch (checkFileStatus(uriBuffer))
	{
	case NOT_EXIST:
		sprintf(outputBuffer, "HTTP/1.1 404 Not Found\r\n\r\n");
		write(fd, outputBuffer, strlen(outputBuffer));
		exit(3);
		break;

	case IS_DIR:
		if (uriBuffer[urilen-1] != '/')
		{
			uriBuffer[urilen] = '/';
			uriBuffer[urilen+1] = 0;
			sprintf(outputBuffer, "HTTP/1.1 301 Moved Permanently\r\n"
				              "Location: %s:%d/%s\r\n\r\n",
				              siteUri, portNumber, uriBuffer);
			write(fd, outputBuffer, strlen(outputBuffer));
			exit(3);
		}
		
		sprintf(uriBuffer2, "%sindex.html", uriBuffer);

		switch (checkFileStatus(uriBuffer2)) {
			case NOT_EXIST:
			case IS_DIR:
				outputListing(fd, uriBuffer, outputBuffer);
				/* directory generation */
				break;
			default:
				writeFile(fd, uriBuffer2, outputBuffer);
				break;
		}

		exit(1);
		break;
	}

	writeFile(fd, uriBuffer, outputBuffer);

	exit(1);
}

int main(int argc, char **argv)
{
	int i, pid, listenfd, socketfd;
	size_t length;
	static struct sockaddr_in cli_addr;
	static struct sockaddr_in serv_addr;

	if(argc <= 2)
	{
		fprintf(stderr, "Error: Missing Path an port number.\n");
		exit(4);
	}

	/* change dir */
	if(chdir(argv[1]) == -1){ 
		printf("ERROR: Can't Change to directory %s\n",argv[2]);
		exit(4);
	}

	sscanf(argv[2], "%d", &portNumber);


	/* 讓父行程不必等待子行程結束 */
	signal(SIGCLD, SIG_IGN);


	/* open socket */
	if ((listenfd=socket(AF_INET, SOCK_STREAM,0))<0)
		exit(3);



	/* configs */
	serv_addr.sin_family = AF_INET;
	/* use any inaddr */
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);



	/* use assigned port number */
	serv_addr.sin_port = htons(portNumber);


	/* init listener */
	if (bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr))<0)
		exit(3);

	/* beginning listening */
	if (listen(listenfd,64)<0)
		exit(3);

	while(1) {
		length = sizeof(cli_addr);
		/* wait for connection */
		if ((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, (socklen_t *) &length))<0)
			exit(3);

		/* fork child */
		if ((pid = fork()) < 0) {
			exit(3);
		} else {
			if (pid == 0) {  /* child */
				close(listenfd);
				handle_socket(socketfd);
			} else { /* parent */
				close(socketfd);
			}
		}
	}
}