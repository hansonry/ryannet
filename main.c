#include "ryannet.h"
#include <stdio.h>

int main(int args, char * argc[])
{
   struct ryannet_socket_tcp * client_socket, * server_socket, * con;
   char buffer[255];
   int size;
   
   server_socket = ryannet_socket_tcp_new();
   ryannet_socket_tcp_bind(server_socket, NULL, "1234");

   client_socket = ryannet_socket_tcp_new();
   ryannet_socket_tcp_connect(client_socket, "localhost", "1234");

   con = ryannet_socket_tcp_accept(server_socket);

   size = sprintf(buffer, "Hey, Whazzup!\n") + 1;
   ryannet_socket_tcp_send(con, buffer, size);

   size = ryannet_socket_tcp_receive(client_socket, buffer, 255);

   printf("Message Length %d, Messgee: %s\n", size, buffer);


   ryannet_socket_tcp_destroy(con);
   ryannet_socket_tcp_destroy(client_socket);
   ryannet_socket_tcp_destroy(server_socket);
   printf("End\n");
   return 0;
}
