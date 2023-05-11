#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <sys/time.h>

#define SA struct sockaddr

typedef struct
{
  unsigned char id;
  unsigned int numElmt;
  union mydata
  {
    double dElmt;
    unsigned int iElmt;
  } val[25];
} MyMsg_t;

uint64_t htonll(uint64_t x)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return ((uint64_t)htonl(x & 0xFFFFFFFF) << 32) | htonl(x >> 32);
#else
  return x;
#endif
}

uint64_t ntohll(uint64_t x)
{
#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
  return ((uint64_t)ntohl(x & 0xFFFFFFFF) << 32) | ntohl(x >> 32);
#else
  return x;
#endif
}

double htond(double x)
{
  int num = 1;
  if (*(char *)&num == 1)
  {
    union
    {
      uint64_t i;
      double d;
    } value = {.d = x};

    value.i = htonll(value.i);

    return value.d;
  }
  else
  {
    return x;
  }
}

double ntohd(double x)
{
  int num = 1;
  if (*(char *)&num == 1)
  {
    union
    {
      uint64_t i;
      double d;
    } value = {.d = x};

    value.i = ntohll(value.i);

    return value.d;
  }
  else
  {
    return x;
  }
}

int sockfd, to_exit = 0, i, select_no;
socklen_t servAddrLen;
struct sockaddr_in cliAddr, servAddr;

void ConvertToNbw(MyMsg_t *msg, int num)
{
  int i;
  unsigned int tmp;
  msg->numElmt = htond(num);
  for (i = 0; i < num; i++)
  {
    tmp = htond(msg->val[i].iElmt);
    msg->val[i].iElmt = tmp;
  }
}

int main(int argc, char **argv)
{
  MyMsg_t txMsg;
  unsigned char txbuff[80];
  char rxMsg[10];
  int i, j, select_no, servAddrLen;
  fd_set readfd;

  if (argc != 3)
  {
    printf("Usage : <myclient> <server IP address> <server port>\n");
    exit(0);
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  if (sockfd < 0)
  {
    switch (errno)
    {
    case EACCES:
      printf("Permission to create socket of the specified type is denied\n");
      break;

    case EAFNOSUPPORT:
      printf("Socket of given address family not supported\n");
      break;

    case EINVAL:
      printf("Invalid values in type\n");
      break;

    default:
      printf("Other socket errors\n");
      break;
    }
    exit(0);
  }

  bzero(&servAddr, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  inet_pton(AF_INET, argv[1], &servAddr.sin_addr);
  printf("%x\n", ntohl(servAddr.sin_addr.s_addr));
  servAddr.sin_port = htons(atoi(argv[2]));

  srand(time(NULL));

  servAddrLen = sizeof(servAddr);
  sendto(sockfd, "INITIAL", strlen("INITIAL") + 1, 0, (SA *)&servAddr, servAddrLen);
  double v;
  if (recvfrom(sockfd, &v, sizeof(v), 0, (SA *)&servAddr, &servAddrLen) < 0)
  {
    perror("Error receiving v from the server");
    exit(EXIT_FAILURE);
  }
  v = ntohd(v);

  for (;;)
  {
    for (i = 0; i < 10; i++)
    {
      txMsg.val[i].dElmt = (drand48() * 2 * v) - v;
      printf("%e ", txMsg.val[i].dElmt);
    }
    printf("\n");

    ConvertToNbw(&txMsg, 10);

    sendto(sockfd, &txMsg, sizeof(txMsg), 0, (struct sockaddr *)&servAddr, sizeof(servAddr));
    bzero(rxMsg, sizeof(rxMsg));
    recvfrom(sockfd, &rxMsg, sizeof(rxMsg), 0, (struct sockaddr *)&servAddr, &servAddrLen);

    fputs(rxMsg, stdout);
    if (!strncmp(rxMsg, "NEXT MSG", sizeof(rxMsg)))
    {
      printf("Server->client : ");
      fputs(rxMsg, stdout);
      printf("\n");
    }
    else
    {
      printf("Terminated from server\n");
      break;
    }
  }
}
