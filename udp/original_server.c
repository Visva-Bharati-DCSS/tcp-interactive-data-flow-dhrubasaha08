#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <unistd.h>

#define SA struct sockaddr
#define MAX_CLIENTS 256

typedef struct
{
  unsigned char id;
  unsigned int numElmt;
  union mydata
  {
    double dElmt;
    unsigned int iElmt;
  } arr[25];
} MyMsg_t;

typedef struct
{
  unsigned char id;
  double last_five_values[5];
  int value_index;
  int num_values_received;
  double running_sum;
  double v;
} ClientInfo;
ClientInfo clients[MAX_CLIENTS] = {0};

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

void ConvertToHostByteOrder(MyMsg_t *msg)
{
  int i;
  msg->numElmt = ntohd(msg->numElmt);

  for (i = 0; i < (msg->numElmt); i++)
    msg->arr[i].iElmt = ntohd(msg->arr[i].iElmt);

  for (i = 0; i < msg->numElmt; i++)
    printf("%e ", msg->arr[i].dElmt);

  printf("\n");
}

unsigned char generate_client_id(struct sockaddr_in *client_addr)
{
  unsigned int ip = ntohl(client_addr->sin_addr.s_addr);
  unsigned short port = ntohs(client_addr->sin_port);
  return (unsigned char)((ip ^ port) % 256);
}

void archive_instance(unsigned char client_id, double running_average)
{
  FILE *archive_file = fopen("archived_instances.txt", "a");
  if (archive_file == NULL)
  {
    perror("Error opening archive file");
    return;
  }
  fprintf(archive_file, "Client ID: %u, Running Average: %f\n", client_id, running_average);
  fclose(archive_file);
}

void update_client_info(ClientInfo *client_info, MyMsg_t *msg)
{
  int i;

  for (i = 0; i < msg->numElmt; i++)
  {
    double new_value = msg->arr[i].dElmt;

    // Update the running sum and last_five_values array
    client_info->running_sum -= client_info->last_five_values[client_info->value_index];
    client_info->running_sum += new_value;
    client_info->last_five_values[client_info->value_index] = new_value;

    // Move to the next index in the last_five_values array
    client_info->value_index = (client_info->value_index + 1) % 5;

    // Increment the number of values received
    client_info->num_values_received++;

    // Calculate the running average
    int num_values_to_average = client_info->num_values_received < 5 ? client_info->num_values_received : 5;
    double running_average = client_info->running_sum / num_values_to_average;

    // If the running average is above 0.75 * v, archive the instance
    if (running_average > 0.75 * client_info->v)
    {
      printf("Archiving instance for client ID %u\n", client_info->id);
      // Add the instance to the archive (e.g., a list or a file)
      archive_instance(client_info->id, running_average);
    }
  }
}

int main(int argc, char *argv[])
{
  MyMsg_t rxMsg;
  char *txMsg = "NEXT MSG";
  int sockfd, numRxMsg;
  unsigned short cliPort;
  struct sockaddr_in servAddr, cliAddr;
  char cli_ip_addr[16];
  socklen_t cliAddrLen;

  if (argc != 3)
  {
    printf("Usage : <server> <server port> <v>\n");
    exit(0);
  }

  sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  bzero(&servAddr, sizeof(servAddr));
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(atoi(argv[1]));
  numRxMsg = atoi(argv[2]);

  if (bind(sockfd, (struct sockaddr *)&servAddr, sizeof(servAddr)) < 0)
  {
    if (errno == EADDRINUSE)
      printf("Bind error ... Port is still in use\n");
    else
      printf("Bind error ... \n");
    exit(0);
  }

  cliAddrLen = sizeof(cliAddr);

  bzero(cli_ip_addr, 16);

  while (recvfrom(sockfd, &rxMsg, sizeof(rxMsg), 0, (struct sockaddr *)&cliAddr, &cliAddrLen))
  {
    inet_ntop(AF_INET, &(cliAddr.sin_addr), &cli_ip_addr[0], 16);
    printf("Client IP address : ");
    puts(cli_ip_addr);
    cliPort = ntohs(cliAddr.sin_port);
    printf("Client port id : %u\n", cliPort);
    ConvertToHostByteOrder(&rxMsg);

    unsigned char client_id = generate_client_id(&cliAddr);

    if (clients[client_id].id == 0)
    {
      clients[client_id] = (ClientInfo){
          .id = client_id,
          .value_index = 0,
          .num_values_received = 0,
          .running_sum = 0.0,
          .v = atof(argv[2])};

      double v_network_order = htond(clients[client_id].v);
      sendto(sockfd, &v_network_order, sizeof(v_network_order), 0, (struct sockaddr *)&cliAddr, cliAddrLen);
    }

    update_client_info(&clients[client_id], &rxMsg);

    numRxMsg--;
    if (numRxMsg == 0)
    {
      sendto(sockfd, "STOP", 5, 0, (struct sockaddr *)&cliAddr, cliAddrLen);
      break;
    }
    else
    {
      sleep(1);
      printf("Asking for next message\n");
      sendto(sockfd, txMsg, strlen(txMsg) + 1, 0, (struct sockaddr *)&cliAddr, cliAddrLen);
    }
  }
  printf("%x\n", cliAddr.sin_addr.s_addr);
}
