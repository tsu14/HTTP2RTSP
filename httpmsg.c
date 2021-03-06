#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "httpmsg.h"
#include "socketfunc.h"


/* TODO: Get message could include row "Range: bytes=x-y" to
 * request chuncks */
int http_get(const char *url, unsigned char *buffer)
{
  char path[URLSIZE], host[URLSIZE];

  memset(buffer, 0, BUFSIZE);

  parse_url(url, host, path);
 
 /* Print the HTTP GET in its correct format */
 snprintf((char *)buffer, BUFSIZE, 
     "GET %s HTTP/1.1\r\nUser-Agent: http2rtsp\r\nHost: %s\r\nAccept: */*\r\n\r\n",
     path, host);

 return strlen((char *)buffer);

}


void parse_url(const char *url, char *host, char *path)
{
  char *temphost;
  char *temppath = (char *)url;
  int i;

  memset(host, 0, URLSIZE);
  memset(path, 0, URLSIZE);

  /* If a http:// prefix is found search for the third occurence of "/",
   * otherwise the first */ 
  if (strncasecmp("http://", url, 7) == 0) {
    for (i = 0; i < 3; i++) {
      temppath = strchr(temppath, '/') + 1;
    }
    temppath -= 1;
    temphost = (char *)url + 7;
  }
  else {
    temppath = strchr(url, '/');
    temphost = (char *)url;
  }

  /* Write the host and the path parts to the given parameters */
  snprintf(host, URLSIZE, "%.*s", (int)(temppath - temphost), temphost);
  snprintf(path, URLSIZE, "%s", temppath);
}


int parse_rtsp(RTSPMsg *msg, const unsigned char *buffer)
{
  char *temp = (char *)buffer;
  char *a;
  char *end = strstr((char *)buffer, "\r\n\r\n");
  int typeflag = 0;

  memset(msg, 0, sizeof(RTSPMsg));

  while (temp && temp < end) {

    if (!typeflag) {

      if (strncmp(temp, "OPTIONS", 7) == 0) {
        msg->type = OPTIONS;
      }
      else if (strncmp(temp, "DESCRIBE", 8) == 0) {
        msg->type = DESCRIBE;
      }
      else if (strncmp(temp, "SETUP", 5) == 0) {
        msg->type = SETUP;
      }
      else if (strncmp(temp, "PLAY", 4) == 0) {
        msg->type = PLAY;
      }
      else if (strncmp(temp, "TEARDOWN", 8) == 0) {
        msg->type = TEARDOWN;
      }
      else if (strncmp(temp, "RTSP/1.0 200 OK", 15) == 0) {
        msg->type = OK;
      }
      else return -1;

      msg->fields |= F_TYPE;
      typeflag = 1;
    }

    if (strncmp(temp, "CSeq: ", 6) == 0) {
      sscanf(temp, "CSeq: %d\r\n", &(msg->cseq));
      msg->fields |= F_CSEQ;
    }
    else if (strncmp(temp, "Session: ", 9) == 0) {
      sscanf(temp, "Session: %d\r\n", &(msg->session));
      msg->fields |= F_SESSION;
    }
    else if (strncmp(temp, "Content-Length: ", 16) == 0) {
      sscanf(temp, "Content-Length: %d\r\n", &(msg->contentlen));
      msg->fields |= F_CONTLEN;
    }
    else if (strncmp(temp, "Content-Type: ", 14) == 0) {
      sscanf(temp, "Content-Type: %s\r\n", (char *)&(msg->contenttype));
      msg->fields |= F_CONTTYPE;
    }
    else if (strncmp(temp, "Transport: ", 11) == 0) {
      a = strstr(temp, "client_port=");
      sscanf(temp + 11, "%s\r\n", (char *)&(msg->transport));
      sscanf(a + 12, "%[0-9]-%[0-9]", (char *)&(msg->clirtpport), (char *)&(msg->clirtcpport));
      msg->fields |= F_TRANSPORT;
    }

    temp = strstr(temp, "\r\n") + 2;
  }

  return 1;
}



int write_rtsp(const RTSPMsg *msg, unsigned char *buffer)
{
  char *temp = (char *)buffer;

  memset(temp, 0, BUFSIZE);
  
  sprintf(temp, "RTSP/1.0 200 OK\r\n");
  temp += strlen(temp);

  if (msg->fields & F_CSEQ) {
    sprintf(temp, "CSeq: %d\r\n", msg->cseq);
    temp += strlen(temp);
  }

  if (msg->fields & F_DATE) {
    sprintf(temp, "Date: %s\r\n", msg->date);
    temp += strlen(temp);
  }

  if (msg->fields & F_SESSION) {
    sprintf(temp, "Session: %d\r\n", msg->session);
    temp += strlen(temp);
  }

  if (msg->fields & F_CONTTYPE) {
    sprintf(temp, "Content-Type: %s\r\n", msg->contenttype);
    temp += strlen(temp);
  }

  if (msg->fields & F_CONTLEN) {
    sprintf(temp, "Content-Length: %d\r\n", msg->contentlen);
    temp += strlen(temp);
  }

  if (msg->fields & F_TRANSPORT) {
    sprintf(temp, "Transport: %s\r\n", msg->transport);
    temp += strlen(temp);
  }

  sprintf(temp, "\r\n");
  temp += 2;

  if (msg->fields & F_DATA) {
    sprintf(temp, "%s", msg->data);
    temp += strlen(temp);
  }


  return temp - (char *)buffer;

}


int rtsp_options(const RTSPMsg *msg, unsigned char *buf)
{
  RTSPMsg newmsg;
  char *temp;
  
  memset(&newmsg, 0, sizeof(RTSPMsg));
  newmsg = *msg;
  temp = (char *)buf + write_rtsp(&newmsg, buf) - 2;
  sprintf(temp, "Public: DESCRIBE, SETUP, PLAY, TEARDOWN\r\n\r\n");
  
  return strlen((char *)buf);
} 
/* TODO: The date format is probably wrong, lets see if it works anyway */
int rtsp_describe(const RTSPMsg *msg, unsigned char *buf)
{
  char timebuf[50];
  time_t timeint;
  RTSPMsg newmsg = *msg;

  memset(timebuf, 0, 50);
  time(&timeint);
  ctime_r(&timeint, timebuf);
  timebuf[strlen(timebuf) - 1] = 0;

  sprintf(newmsg.date, "%s", timebuf);
  sprintf(newmsg.contenttype, "application/sdp");
  sprintf(newmsg.data, 
      "v=0\r\no=atte\r\ns=mpeg4video\r\n"
      "t=0 0\r\na=recvonly\r\nm=video 40404 RTP/AVP 96\r\n"
      "a=rtpmap:96 H264/90000\r\n");

  newmsg.contentlen = strlen(newmsg.data);
  newmsg.fields |= (F_DATE | F_CONTTYPE | F_DATA | F_CONTLEN);

  return write_rtsp(&newmsg, buf);
  
}


int rtsp_setup(const RTSPMsg *msg, unsigned char *buf, int rtpport, int rtcpport)
{
  RTSPMsg newmsg = *msg;
  newmsg.session = (rand() % 1000000);
  newmsg.fields |= F_SESSION;
  sprintf(newmsg.transport + strlen(newmsg.transport), ";server_port=%d-%d", rtpport, rtcpport);
  
  return write_rtsp(&newmsg, buf);
}




















