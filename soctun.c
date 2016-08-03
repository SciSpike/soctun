#include <sys/ioctl.h>
#include <netinet/tcp.h>
#include <sys/sys_domain.h>
#include <sys/kern_control.h>
#include <net/if_utun.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/un.h>
#include <netdb.h>
#include <stdlib.h>

/*
 * soctun - connects to socat style socket for tunneling into a private network.
 *
 * Inspiration: http://newosxbook.com/src.jl?tree=listings&file=17-15-utun.c by Jonathan Levin.
 *
 * Jonathan Kamke SciSpike LLC July 15, 2016
 */

int tun(int unit)
{
  struct sockaddr_ctl sc;
  struct ctl_info ctlInfo;
  int fd;

  memset(&ctlInfo, 0, sizeof(ctlInfo));
  if (strlcpy(ctlInfo.ctl_name, UTUN_CONTROL_NAME, sizeof(ctlInfo.ctl_name))
      >= sizeof(ctlInfo.ctl_name))
  {
    fprintf(stderr, "UTUN_CONTROL_NAME too long");
    return -1;
  }
  fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);

  if (fd == -1)
  {
    perror("socket(SYSPROTO_CONTROL)");
    return -1;
  }
  if (ioctl(fd, CTLIOCGINFO, &ctlInfo) == -1)
  {
    perror("ioctl(CTLIOCGINFO)");
    close(fd);
    return -1;
  }

  sc.sc_id = ctlInfo.ctl_id;
  sc.sc_len = sizeof(sc);
  sc.sc_family = AF_SYSTEM;
  sc.ss_sysaddr = AF_SYS_CONTROL;
  sc.sc_unit = unit;

  // If the connect is successful, a tun%d device will be created

  if (connect(fd, (struct sockaddr *) &sc, sizeof(sc)) == -1)
  {
    perror("connect(AF_SYS_CONTROL)");
    close(fd);
    return -1;
  }
  return fd;
}

int unix(char *path)
{
  int ufd;
  int cfd;
  socklen_t clen;
  struct sockaddr_un addr;

  ufd = socket(AF_UNIX, SOCK_STREAM, 0);
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
  int c = connect(ufd, (struct sockaddr*) &addr, sizeof(addr));
  if (c == -1)
  {
    perror("connect(ufd)");
    return -1;
  }
  return ufd;
}
int unixServer(char *path)
{
  int ufd;
  int cfd;
  socklen_t clen;
  struct sockaddr_un addr;
  struct sockaddr_un caddr;

  ufd = socket(AF_UNIX, SOCK_STREAM, 0);
  memset(&addr, 0, sizeof(addr));
  addr.sun_family = AF_UNIX;
  strncpy(addr.sun_path, path, sizeof(addr.sun_path) - 1);
  unlink(addr.sun_path);
  int c = bind(ufd, (struct sockaddr*) &addr, sizeof(addr));
  if (c == -1)
  {
    perror("bind(ufd)");
    return -1;
  }
  listen(ufd, 5);
  clen = sizeof(caddr);
  cfd = accept(ufd, (struct sockaddr *) &caddr, &clen);
  return cfd;
}

int tcp(char *hostname, int portno, int noDelayFlag)
{
  int sockfd;
  struct sockaddr_in serveraddr;
  struct hostent *server;

  /* socket: create the socket */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    perror("ERROR opening socket");

  /* gethostbyname: get the server's DNS entry */
  server = gethostbyname(hostname);
  if (server == NULL)
  {
    fprintf(stderr, "ERROR, no such host as %s\n", hostname);
    exit(0);
  }

  /* build the server's Internet address */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char *) server->h_addr, (char *)&serveraddr.sin_addr.s_addr, server->h_length);
  serveraddr.sin_port = htons(portno);
  setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, &noDelayFlag,
      sizeof noDelayFlag);
  /* connect: create a connection with the server */
  if (connect(sockfd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0)
  {
    perror("connect(ufd)");
    return -1;
  }
  return sockfd;
}
static void usage(char *name)
{
  fprintf(stderr,
      "usage: %s: [-nv] [-h hostname] [-m MTU] [-p port] [-t tunX] [-u unix socket path]\n",
      name);
  exit(1);
}

void printSome(unsigned char c[], int len, char *pipe)
{
  for (int i = 0; i < len; i++)
  {
    if (i % 0x10 == 0)
    {
      fprintf(stderr, "\n%s %04x ", pipe, i);
    }
    if (i % 8 == 0)
    {
      fprintf(stderr, " ");
    }
    fprintf(stderr, "%02x ", c[i]);
  }
  fprintf(stderr, "\n\n");

}
int main(int argc, char *argv[])
{
  int header = 4;
  int mtu = 1500;
  int debug = 0;
  char *hostname;
  char *path;
  int port;
  int noDelayFlag = 0;
  int tid;
  int ch;
  int utunfd;
  int remoteFd;
  pid_t pid;

  while ((ch = getopt(argc, argv, "h:m:np:t:u:v")) != -1)
    switch (ch)
    {
    case 'h':
      hostname = optarg;
      break;
    case 'm':
      mtu = atoi(optarg) + header;
      break;
    case 'n':
      noDelayFlag = 1;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 't':
      tid = atoi(optarg) + 1;
      break;
    case 'u':
      path = optarg;
      break;
    case 'v':
      debug++;
      break;
    default:
      usage(argv[0]);
    }

  int max = mtu + header;
  utunfd = tun(tid);
  if (port > 0)
  {
    remoteFd = tcp(hostname, port, noDelayFlag);
  }
  else
  {
    remoteFd = unix(path);
  }
  fd_set readset;

  int maxfd = remoteFd;
  if (maxfd < utunfd)
    maxfd = utunfd;

  if (utunfd == -1 || remoteFd == -1)
  {
    fprintf(stderr, "Unable to establish UTUN/IPC descriptors - aborting\n");
    exit(1);
  }

  unsigned char fromRemote[max];
  unsigned char fromTun[max];
  int len;
  int expected = 0;
  int totalLen = 0;
  int remaining = 0;
  for (;;)
  {
    FD_ZERO(&readset);
    FD_SET(utunfd, &readset);
    FD_SET(remoteFd, &readset);

    select(maxfd + 1, &readset, NULL, NULL, NULL);

    if (FD_ISSET(utunfd, &readset))
    {
      len = read(utunfd, fromTun, max);

      if (len > 0)
      {
        if (debug)
        {
          printSome(fromTun, len, ">>");
        }
        //set frame
        fromTun[0] = 0;
        fromTun[1] = 0;
        fromTun[2] = 8;
        fromTun[3] = 0;
        write(remoteFd, fromTun, len);
      }
      else if (len == 0)
      {
        break;
      }
    }

    if (FD_ISSET(remoteFd, &readset))
    {
      //we always have remaining, set to header length
      if (!expected && !remaining && !totalLen)
      {
        //frame + packet header
        remaining = 8;
      }

      len = read(remoteFd, &fromRemote[totalLen], remaining);
      remaining = remaining - len;
      totalLen += len;

      //got it all, but just the header, parse expected
      if (!expected && !remaining)
      {
        remaining = expected = (fromRemote[6] << 8) | (fromRemote[7] & 0xff);
        remaining = remaining - header;
        if (debug)
        {
          fprintf(stderr,
              "Finished reading header totalLen %i pExpected %i remaining %i\n",
              totalLen, expected, remaining);
          printSome(fromRemote, totalLen, "<<");
        }
      }
      //really got it all, ship it
      else if (expected && !remaining)
      {
        if (debug)
        {
          fprintf(stderr,
              "Finished reading packet totalLen %i pExpected %i remaining %i\n",
              totalLen, expected, remaining);
          printSome(fromRemote, totalLen, "<<");
        }
        //set frame
        fromRemote[0] = 0;
        fromRemote[1] = 0;
        fromRemote[2] = 0;
        fromRemote[3] = 2;
        write(utunfd, fromRemote, totalLen);
        //we just completed a packet, so reset
        expected = totalLen = remaining = 0;
      }
      else if (len == 0)
      {
        break;
      }
      else if (debug)
      {
        fprintf(stderr,
            "Keep reading packet totalLen %i pExpected %i remaining %i\n",
            totalLen, expected, remaining);
      }
    }
  }
  close(utunfd);
  close(remoteFd);
  return (0);
}
