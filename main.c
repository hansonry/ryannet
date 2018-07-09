#include "ryannet.h"
#include <stdio.h>

#define PORT "1234"
int main(int argc, char * args[])
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
   ryannet_socket_tcp_destroy(server_socket);
   ryannet_socket_tcp_destroy(client_socket);

   sprintf(buffer, "This is not the message you want");

   if( argc == 3)
   {
      if(args[1][0] == 's')
      {
         server_socket = ryannet_socket_tcp_new();
         ryannet_socket_tcp_bind(server_socket, NULL, args[2]);
         addy = ryannet_socket_tcp_get_address_local(server_socket);
         printf("Local ( %s : %s)\n", ryannet_address_get_address(addy), ryannet_address_get_port(addy));
         
         con = NULL;
         while(con == NULL)
         {
            con = ryannet_socket_tcp_accept_nonblock(server_socket);
            //printf("Accept Loop\n");
         }

         size = sprintf(buffer, "Hey, Whazzup!") + 1;
         ryannet_socket_tcp_send(con, buffer, size);
         ryannet_socket_tcp_destroy(con);
         ryannet_socket_tcp_destroy(server_socket);
      }
      else
      {
         client_socket = ryannet_socket_tcp_new();
         ryannet_socket_tcp_connect(client_socket, args[1], args[2]);
         size = 0;
         while(size == 0)
         {
            size = ryannet_socket_tcp_receive_nonblock(client_socket, buffer, 255);
            printf("Recv Loop\n");
         }

         printf("Message Length %d, Messgee: %s\n", size, buffer);
         ryannet_socket_tcp_destroy(client_socket);

      }
   }


   ryannet_destroy();
   printf("End\n");
   return 0;
}
