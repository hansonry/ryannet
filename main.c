#include "ryannet.h"
#include <stdio.h>

#define PORT "1234"
int main(int args, char * argc[])
{
   struct ryannet_socket_tcp * client_socket, * server_socket, * con;
   struct ryannet_address * addy;
   char buffer[255];
   int size;
   int rv;
   
   (void)ryannet_init();
   
   server_socket = ryannet_socket_tcp_new();
   rv = ryannet_socket_tcp_bind(server_socket, NULL, PORT);
   //printf("Return Value %d\n", rv);
   
   client_socket = ryannet_socket_tcp_new();
   rv = ryannet_socket_tcp_connect(client_socket, "localhost", PORT);
   //printf("Return Value %d\n", rv);

   con = ryannet_socket_tcp_accept(server_socket);

   size = sprintf(buffer, "Hey, Whazzup!") + 1;
   ryannet_socket_tcp_send(con, buffer, size);

   sprintf(buffer, "This is not the message you want");
   size = ryannet_socket_tcp_receive(client_socket, buffer, 255);

   printf("Message Length %d, Messgee: %s\n", size, buffer);

   addy = ryannet_socket_tcp_get_address_remote(client_socket);
   printf("Remote ( %s : %s)\n", ryannet_address_get_address(addy), ryannet_address_get_port(addy));
   addy = ryannet_socket_tcp_get_address_local(client_socket);
   printf("Local ( %s : %s)\n", ryannet_address_get_address(addy), ryannet_address_get_port(addy));
   
   
   ryannet_socket_tcp_destroy(con);
   ryannet_socket_tcp_destroy(client_socket);
   ryannet_socket_tcp_destroy(server_socket);
   ryannet_destroy();
   printf("End\n");
   return 0;
}
