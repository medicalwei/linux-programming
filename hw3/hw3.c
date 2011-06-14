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

char pageContainerHeader[] = 
"Content-Type: text/html; charset=utf-8\r\n\r\n"
"<!DOCTYPE html>\n"
"<html>\n"
"<head>\n"
"	<meta http-equiv=\"content-type\" content=\"text/html; charset=utf-8\">\n"
"	<title>%s</title>\n"
"	<style>html{font-family:\"Helvetica Neue\",\"Helvetica\",\"Nimbus Sans\",\"Arial\",sans-serif;color:#666;background:url(\"artworks/ntou.png\") 95\% bottom no-repeat #ccc;min-height:100\%;}body{width:720px;margin:0 auto;padding:0 0 40pt;font-weight:300;}h1{font-weight:500;text-shadow:0 2px 0 rgba(0,0,0,0.2),0 -1px 0 rgba(255,255,255,0.2);color:#666;margin-top:40pt;}a{color:#333;border-bottom:1px solid #ccc;text-decoration:none;-webkit-transition:border-color 0.3s linear;-moz-transition:border-color 0.3s linear;-o-transition:border-color 0.3s linear;transition:border-color 0.3s linear;}a:hover{border-bottom:1px solid #999;-webkit-transition:border-color 0.1s linear;-moz-transition:border-color 0.1s linear;-o-transition:border-color 0.1s linear;transition:border-color 0.1s linear;}</style>\n"
"</head>\n"
"<body>\n";

char errorContainerContent[] =
"	<h1>%s</h1>"
"	<p>%s</p>";

char listContainerHeader[] =
"	<h1>%s</h1>\n"
"	<ul id=\"listing\">\n";

char listContainerContent[] =
"		<li><a href=\"%s\">%s</a></li>\n";

char listContainerFooter[] =
"	</ul>\n";

char pageContainerFooter[] =
"</body>\n"
"</html>\n";

char *parameterBuffer=NULL, *inputBuffer=NULL;

#define WRITE_OK 16
#define FORBIDDEN 8
#define FILE_NOT_SUPPORTED 4
#define NOT_EXIST 2
#define IS_DIR 1
#define IS_FILE 0

#define GET 1
#define POST 2
#define PUT 3
#define DELETE 4

int outputErrorMessage (int fd, char outputBuffer[], char *errorTitle, char *errorContent)
{
	sprintf(outputBuffer, pageContainerHeader, errorTitle);
	write(fd, outputBuffer, strlen(outputBuffer));
	sprintf(outputBuffer, errorContainerContent, errorTitle, errorContent);
	write(fd, outputBuffer, strlen(outputBuffer));
	strcpy(outputBuffer, pageContainerFooter);
	write(fd, outputBuffer, strlen(outputBuffer));
	return 0;
}

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

	sprintf(outputBuffer, "HTTP/1.1 200 OK\r\n");
	write(fd, outputBuffer, strlen(outputBuffer));
	sprintf(directoryName, "Index of /%s", directoryBuffer);
	sprintf(outputBuffer, pageContainerHeader, directoryName);
	write(fd, outputBuffer, strlen(outputBuffer));
	sprintf(outputBuffer, listContainerHeader, directoryName);
	write(fd, outputBuffer, strlen(outputBuffer));

	while ((ep = readdir (dp)) != NULL)
	{
		if (ep->d_name[0] == '.')
		{
			continue;
		}
		sprintf(outputBuffer, listContainerContent,
			 ep->d_name,
			 ep->d_name
			);
		write(fd, outputBuffer, strlen(outputBuffer));
	}
	(void) closedir (dp);

	strcpy(outputBuffer, listContainerFooter);
	write(fd, outputBuffer, strlen(outputBuffer));
	strcpy(outputBuffer, pageContainerFooter);
	write(fd, outputBuffer, strlen(outputBuffer));
	
	return WRITE_OK;
}

int writeFile (int fd, char *uriBuffer, char *outputBuffer)
{
	int buflen = 0;
	char* fstr = NULL;
	int i, ret, len, file_fd;
	char uriBuffer2[BUFSIZE+1];
	FILE *cgi_fd;

	buflen = strlen(uriBuffer);

	/* check filetype */
	for(i=0; extensions[i].ext!=0; i++) {
		len = strlen(extensions[i].ext);
		if(!strncmp(&uriBuffer[buflen-len], extensions[i].ext, len)) {
			fstr = extensions[i].filetype;
			break;
		}
	}

	/* cgi */
	if(strncmp(uriBuffer, "cgi-bin/", 8) == 0)
	{
		strcpy (uriBuffer2, "./");
		strncat (uriBuffer2, uriBuffer, BUFSIZE);
		cgi_fd = popen (uriBuffer2, "r");
		if(!cgi_fd)
		{
			sprintf(outputBuffer,"HTTP/1.1 403 Forbidden\r\n");
			write(fd, outputBuffer, strlen(outputBuffer));
			outputErrorMessage(fd, outputBuffer, "403 — Forbiden", "The web server doesn't have permission to access this page.");
			return FORBIDDEN;
		}
		sprintf(outputBuffer,"HTTP/1.1 200 OK\r\n");
		write(fd, outputBuffer, strlen(outputBuffer));
		while (fgets(outputBuffer, BUFSIZE, cgi_fd)) {
			write(fd, outputBuffer, strlen(outputBuffer));
		}
		fclose(cgi_fd);
		return WRITE_OK;
	}
	/* open file */
	else if (!(file_fd=open(uriBuffer, O_RDONLY))) {
		sprintf(outputBuffer,"HTTP/1.1 403 Forbidden\r\n");
		write(fd, outputBuffer, strlen(outputBuffer));
		outputErrorMessage(fd, outputBuffer, "403 — Forbiden", "The web server doesn't have permission to access this page.");
		return FORBIDDEN;
	}


	/* file unsupported */
	if(fstr == 0) {
		/* return FILE_NOT_SUPPORTED; */
		sprintf(outputBuffer,"HTTP/1.1 200 OK\r\nContent-Type: application/octet-stream\r\n\r\n");
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

	close(file_fd);

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
	int urilen;
	long i, ret;
	static char cmdBuffer[BUFSIZE+1], uriBuffer2[BUFSIZE+1];
	static char outputBuffer[BUFSIZE+1];
	static char *uriBuffer;
	int requestType;

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
	
	/* answer POST requests when it is CGI */
	if (strncmp(cmdBuffer,"GET ",4) == 0 || strncmp(cmdBuffer,"get ",4) == 0)
	{
		setenv("REQUEST_METHOD", "GET" , 1);
		uriBuffer = &cmdBuffer[5];
		requestType = GET;
	}
	else if (strncmp(cmdBuffer,"POST ",5) == 0 || strncmp(cmdBuffer,"post ",5) == 0)
	{
		setenv("REQUEST_METHOD", "POST", 1);
		uriBuffer = &cmdBuffer[6];
		requestType = POST;
	}
	else
	{
		exit(3);
	}
	

	/* use null character */
	for(i=0;i<ret;i++) {
		if(cmdBuffer[i] == ' ') {
			cmdBuffer[i] = 0;
		}

		if(requestType == GET && cmdBuffer[i] == '?' && parameterBuffer == NULL) {
			cmdBuffer[i] = 0;
			parameterBuffer = &cmdBuffer[i+1];
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

	if (requestType == POST)
	{
		for (i=0;i<ret-3;i++)
		{
			if (cmdBuffer[i]==0 && cmdBuffer[i+1]==0 && cmdBuffer[i+2]==0 && cmdBuffer[i+3]==0 && inputBuffer == NULL)
			{
				inputBuffer = &cmdBuffer[i+4];
			}
		}
	}

	if (parameterBuffer)
	{
		setenv("QUERY_STRING", parameterBuffer, 1);
	}

	/* get URI string */
	urilen = strlen(uriBuffer);

	if (urilen == 0)
	{
		sprintf(uriBuffer2, "index.html");

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
		sprintf(outputBuffer, "HTTP/1.1 404 Not Found\r\n");
		write(fd, outputBuffer, strlen(outputBuffer));
		outputErrorMessage(fd, outputBuffer, "404 — Not Found", "The page you requested did not found.");
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
	int pid, listenfd, socketfd;
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
