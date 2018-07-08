#include "ryannet.h"
#include <stdio.h>

int main(int args, char * argc[])
{
   struct ryannet_socket_tcp * client_socket, * server_socket, * con;
   char buffer[255];
   int size;
   int rv;
   
   (void)ryannet_init();
   
   server_socket = ryannet_socket_tcp_new();
   rv = ryannet_socket_tcp_bind(server_socket, NULL, "1234");
   printf("Return Value %d\n", rv);
   
   client_socket = ryannet_socket_tcp_new();
   rv = ryannet_socket_tcp_connect(client_socket, "localhost", "1234");
   printf("Return Value %d\n", rv);

   con = ryannet_socket_tcp_accept(server_socket);

   size = sprintf(buffer, "Hey, Whazzup!") + 1;
   ryannet_socket_tcp_send(con, buffer, size);

   sprintf(buffer, "This is not the message you want");
   size = ryannet_socket_tcp_receive(client_socket, buffer, 255);

   printf("Message Length %d, Messgee: %s\n", size, buffer);


   ryannet_socket_tcp_destroy(con);
   ryannet_socket_tcp_destroy(client_socket);
   ryannet_socket_tcp_destroy(server_socket);
   ryannet_destroy();
   printf("End\n");
   return 0;
}
