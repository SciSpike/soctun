#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/sys_domain.h>
#include <sys/kern_control.h>
#include <net/if_utun.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <unistd.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netdb.h> 

#include <stdlib.h> // exit, etc.

/*
 * soctun - connects to socat style tcp socket for tunneling into a docker network.
 *
 * Inspiration: http://newosxbook.com/src.jl?tree=listings&file=17-15-utun.c by Jonathan Levin.
 *
 * Jonathan Kamke SciSpike LLC July 15, 2016
 */

int tun(int unit) {
  struct sockaddr_ctl sc;
  struct ctl_info ctlInfo;
  int fd;

  memset(&ctlInfo, 0, sizeof(ctlInfo));
  if (strlcpy(ctlInfo.ctl_name, UTUN_CONTROL_NAME, sizeof(ctlInfo.ctl_name))
      >= sizeof(ctlInfo.ctl_name)) {
    fprintf(stderr, "UTUN_CONTROL_NAME too long");
    return -1;
  }
  fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);

  if (fd == -1) {
    perror("socket(SYSPROTO_CONTROL)");
    return -1;
  }
  if (ioctl(fd, CTLIOCGINFO, &ctlInfo) == -1) {
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

  if (connect(fd, (struct sockaddr *) &sc, sizeof(sc)) == -1) {
    perror("connect(AF_SYS_CONTROL)");
    close(fd);
    return -1;
  }
  return fd;
}

//int
//unix (char *path) {
//  int ufd;
//  int cfd;
//  socklen_t clen;
//  struct sockaddr_un addr;
//  struct sockaddr_un caddr;
//
//  ufd = socket(AF_UNIX, SOCK_STREAM, 0);
//  //memset(&addr, 0, sizeof(addr));
//  addr.sun_family = AF_UNIX;
//  //snprintf(addr.sun_path, 100, "%s", path);
//  strncpy(addr.sun_path, path, sizeof(addr.sun_path)-1);
//  unlink(addr.sun_path);
//  int c = bind(ufd, (struct sockaddr*)&addr, sizeof(addr));
//  //int c = connect(ufd, (struct sockaddr*)&addr, sizeof(addr));
//  if (c == -1) {
//    perror ("connect(ufd)");
//    return -1;
//  }
//  listen(ufd, 5);
//  clen = sizeof(caddr);
//  cfd = accept(ufd, (struct sockaddr *)&caddr, &clen);
//  //return ufd;
//  return cfd;
//}
int tcp(char *hostname, int portno) {
  int sockfd;
  struct sockaddr_in serveraddr;
  struct hostent *server;

  /* socket: create the socket */
  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0)
    perror("ERROR opening socket");

  /* gethostbyname: get the server's DNS entry */
  server = gethostbyname(hostname);
  if (server == NULL) {
    fprintf(stderr, "ERROR, no such host as %s\n", hostname);
    exit(0);
  }

  /* build the server's Internet address */
  bzero((char *) &serveraddr, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  bcopy((char *) server->h_addr,
  (char *)&serveraddr.sin_addr.s_addr, server->h_length);
  serveraddr.sin_port = htons(portno);

  /* connect: create a connection with the server */
  if (connect(sockfd, (struct sockaddr*) &serveraddr, sizeof(serveraddr)) < 0) {
    perror("connect(ufd)");
    return -1;
  }
  return sockfd;
}
static void usage(char *name) {
  fprintf(stderr, "usage: %s:  [-h hostname] [-p port] [-t tunX]\n", name);
  exit(1);
}

int main(int argc, char *argv[]) {
  int max = 4096;
  char *hostname;
  int port;
  int tid;
  int ch;
  pid_t pid;

  while ((ch = getopt(argc, argv, "mt:h:p:")) != -1)
    switch (ch) {
    case 't':
      tid = atoi(optarg) + 1;
      break;
    case 'h':
      hostname = optarg;
      break;
    case 'p':
      port = atoi(optarg);
      break;
    case 'm':
      max = atoi(optarg);
      break;
    default:
      usage(argv[0]);
    }

  int utunfd = tun(tid);
  int unixfd = tcp(hostname, port);

  if (utunfd == -1 || unixfd == -1) {
    fprintf(stderr, "Unable to establish UTUN descriptor - aborting\n");
    exit(1);
  }

  pid = fork();
  short t = 1;
  if (pid == 0) {
    while (t) {
      unsigned char c[max];
      int len;
      int i;

      len = read(utunfd, c, max);

      c[0] = 0;
      c[1] = 0;
      c[2] = 8;
      c[3] = 0;
      if (len > 0) {
        write(unixfd, c, len);
      }
      if (len <= 0) {
        t = 0;
      }
    }
  } else {
    while (t) {
      unsigned char c[max];
      int len;
      int i;

      len = read(unixfd, c, max);

      // First 4 bytes of read data are the AF: 2 for AF_INET, 1E for AF_INET6, etc..
      c[0] = 0;
      c[1] = 0;
      c[2] = 0;
      c[3] = 2;
      if (len > 0) {
        write(utunfd, c, len);
      } else {
        t = 0;
      }
    }
    kill(pid, SIGKILL);
  }
  close(utunfd);
  close(unixfd);
  return (0);
}
