#include "ryannet.h"
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#else // _WIN32
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <sys/time.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <unistd.h>
#include <fcntl.h>
#endif // _WIN32
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#ifdef _WIN32
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#endif // _WIN32

// Define Close based on system arch
#ifdef _WIN32
#define ryannet_close(fd) closesocket(fd)
#else // _WIN32
#define ryannet_close(fd) close(fd)
#endif // _WIN32

struct ryannet_address
{
   char * address;
   char * port;
   struct sockaddr_storage raw;
};

struct ryannet_socket_tcp
{
   struct ryannet_address local;
   struct ryannet_address remote;
   int fd;
   int connected_flag;
   int remote_closed_flag;
};

static char * ryannet_string_copy(const char * src)
{
   char * out;
   size_t size;
   if(src == NULL)
   {
      out = NULL;
   }
   else
   {
      size = strlen(src) + 1;
      out = malloc(sizeof(char) * size);
      memcpy(out, src, size);
   }
   return out;
}

int ryannet_init(void)
{
#ifdef _WIN32
   WSADATA wsa_data;
   int rv, wsas_rv;
   wsas_rv = WSAStartup(MAKEWORD(2, 2), &wsa_data);
   if(wsas_rv != 0)
   {
      fprintf(stderr, "WSAStartup Failed with error %d\n", wsas_rv);
      rv = 1;
   }
   else
   {
      rv = 0;
   }
   return rv;
#else // _WIN32
   return 0;
#endif // _WIN32
}

void ryannet_destroy(void)
{
#ifdef _WIN32
   WSACleanup();
#endif // _WIN32
}




struct ryannet_address * ryannet_address_new(const char * address, const char * port)
{
   struct ryannet_address * addy;
   addy = malloc(sizeof(struct ryannet_address));
   addy->address = ryannet_string_copy(address);
   addy->port = ryannet_string_copy(port);
   memset(&addy->raw, 0, sizeof(struct sockaddr_storage));
   return addy;
}

void ryannet_address_destroy(struct ryannet_address * address)
{
   if(address->address != NULL)
   {
      free(address->address);
   }
   if(address->port != NULL)
   {
      free(address->port);
   }
   free(address);
}

const char * ryannet_address_get_address(struct ryannet_address * address)
{
   return address->address;
}

const char * ryannet_address_get_port(struct ryannet_address * address)
{
   return address->port;
}


struct ryannet_socket_tcp * ryannet_socket_tcp_new(void)
{
   struct ryannet_socket_tcp * socket;
   socket = malloc(sizeof(struct ryannet_socket_tcp));
   socket->fd = -1;
   socket->local.address = NULL;
   socket->local.port = NULL;
   memset(&socket->local.raw, 0, sizeof(struct sockaddr_storage));
   socket->remote.address = NULL;
   socket->remote.port = NULL;
   memset(&socket->remote.raw, 0, sizeof(struct sockaddr_storage));
   socket->connected_flag = 0;
   socket->remote_closed_flag = 0;
   return socket;
}

void ryannet_socket_tcp_destroy(struct ryannet_socket_tcp * socket)
{
   if(socket->fd != -1)
   {
      ryannet_close(socket->fd);
   }
   if(socket->local.address != NULL)
   {
      free(socket->local.address);
   }
   if(socket->local.port != NULL)
   {
      free(socket->local.port);
   }
   if(socket->remote.address != NULL)
   {
      free(socket->remote.address);
   }
   if(socket->remote.port != NULL)
   {
      free(socket->remote.port);
   }
   free(socket);
}

static int ryannet_set_nonblocking(int fd)
{
   int rv;
#if _WIN32
   int yes = 1;
   if(ioctlsocket(fd, FIONBIO, &yes) != NOERROR)
   {
      rv = 1;
   }
   else
   {
      rv = 0;
   }
#else // _WIN32
   int flags;
   flags = fcntl(fd, F_GETFL, 0);
   if(fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1)
   {
      rv = 1;
   }
   else
   {
      rv = 0;
   }
#endif // _WIN32
   return rv;
}

static int ryannet_set_tcp_nodelay(int fd)
{
   int rv;
   char yes = 1;
   if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(char)) == -1)
   {
      rv = 1;
   }
   else
   {
      rv = 0;
   }
   return rv;
}

static int ryannet_set_reuseaddr(int fd)
{
   int rv;
   char yes = 1;
   if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(char)) == -1)
   {
      rv = 1;
   }
   else
   {
      rv = 0;
   }
   return rv;
}


#define PORT_STRING_SIZE 8
static void ryannet_fill_address(struct ryannet_address * address)
{
   size_t size;
   int port;

   // Address
   if(address->address != NULL)
   {
      free(address->address);
      address->address = NULL;
   }
   switch(address->raw.ss_family)
   {
   case AF_INET:
      size = INET_ADDRSTRLEN;
      break;
   case AF_INET6:
      size = INET6_ADDRSTRLEN;
      break;
   default:
      size = 0;
      break;
   }

  if(size > 0)
  {
     address->address = malloc(sizeof(char) * size);
     (void)inet_ntop(address->raw.ss_family, &address->raw, 
                     address->address, size);
  }

   // Port
   if(address->port != NULL)
   {
      free(address->port);
      address->port = NULL;
   }

   switch(address->raw.ss_family)
   {
   case AF_INET:
      port = ntohs(((struct sockaddr_in *)&address->raw)->sin_port);
      size = PORT_STRING_SIZE;
      break;
   case AF_INET6:
      port = ntohs(((struct sockaddr_in6 *)&address->raw)->sin6_port);
      size = PORT_STRING_SIZE;
      break;
   default:
      size = 0;
      break;
   }
   if(size > 0)
   {
      address->port = malloc(sizeof(char) * size);
      sprintf(address->port, "%d", port);
   }


}

int ryannet_socket_tcp_connect(struct ryannet_socket_tcp * sock, const char * remote_address, const char * remote_port)
{
   struct addrinfo hints, *servinfo, *p;
   int rv;
   socklen_t length;

   memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   
   rv = getaddrinfo(remote_address, remote_port, &hints, &servinfo);
   if(rv != 0)
   {
      fprintf(stderr, "getaddrinfo %s\n", gai_strerror(rv));
      return 1;
   }

   for(p = servinfo; p != NULL; p = p->ai_next)
   {
      sock->fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if(sock->fd == -1)
      {
         // TODO: Maybe Error Handling
         continue;
      }

      rv = ryannet_set_nonblocking(sock->fd);
      if(rv == 1)
      {
         // TODO: Maybe Error Handling
         ryannet_close(sock->fd);
         sock->fd = -1;
         continue;
      }

      rv = ryannet_set_tcp_nodelay(sock->fd);
      if(rv == 1)
      {
         // TODO: Maybe Error Handling
         ryannet_close(sock->fd);
         sock->fd = -1;
         continue;
      }

      rv = connect(sock->fd, p->ai_addr, p->ai_addrlen);
      if(rv == -1)
      {
         ryannet_close(sock->fd);
         sock->fd = -1;
         // TODO: Maybe Error Handling
         continue;
      }
      break;
   }

   if(sock->fd == -1)
   {
      // TODO: Error Handing
      fprintf(stderr, "Error: Couldn't Connect to %s : %s\n", remote_address, remote_port);
      return 1;
   }


   // Copy Local
   length = sizeof(struct sockaddr_storage);
   getsockname(sock->fd, (struct sockaddr *)&sock->local.raw, &length);
   ryannet_fill_address(&sock->local);
   // Copy Remote
   memcpy(&sock->remote.raw, p->ai_addr, p->ai_addrlen);
   ryannet_fill_address(&sock->remote);
   
   sock->connected_flag = 1;

   freeaddrinfo(servinfo);

   return 0;
   
}

int ryannet_socket_tcp_bind(struct ryannet_socket_tcp * sock, const char * bind_address, const char * bind_port)
{
   struct addrinfo hints, *servinfo, *p;
   int rv;
   socklen_t length;

   memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE; // Use any local IPV4 or IPV6 address I have
   
   rv = getaddrinfo(bind_address, bind_port, &hints, &servinfo);
   if(rv != 0)
   {
      fprintf(stderr, "getaddrinfo %s\n", gai_strerror(rv));
      return 1;
   }

   for(p = servinfo; p != NULL; p = p->ai_next)
   {
      sock->fd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
      if(sock->fd == -1)
      {
         // TODO: Maybe Error Handling
         continue;
      }

      rv = ryannet_set_nonblocking(sock->fd);
      if(rv == 1)
      {
         // TODO: Maybe Error Handling
         ryannet_close(sock->fd);
         sock->fd = -1;
         continue;
      }

      rv = ryannet_set_reuseaddr(sock->fd);
      if(rv == 1)
      {
         // TODO: Maybe Error Handling
         ryannet_close(sock->fd);
         sock->fd = -1;
         continue;
      }

      rv = bind(sock->fd, p->ai_addr, p->ai_addrlen);
      if(rv == -1)
      {
         ryannet_close(sock->fd);
         sock->fd = -1;
         // TODO: Maybe Error Handling
         continue;
      }
      break;
   }

   rv = listen(sock->fd, 10);
   if(rv == -1)
   {
      ryannet_close(sock->fd);
      sock->fd = -1;
      // TODO: Error Handling
   }

   if(sock->fd == -1)
   {
      // TODO: Error Handing
      fprintf(stderr, "Error: Couldn't Bind to %s : %s\n", bind_address, bind_port);
      return 1;
   }

   sock->connected_flag = 1;
   freeaddrinfo(servinfo);

   return 0;
   
}

struct ryannet_socket_tcp * ryannet_socket_tcp_accept(struct ryannet_socket_tcp * socket)
{
   int new_fd;
   socklen_t length;
   struct sockaddr_storage remote_address;
   struct ryannet_socket_tcp * new_socket;
   length = sizeof(struct sockaddr_storage);
   new_fd = accept(socket->fd, (struct sockaddr *)&remote_address, &length);
   if(new_fd == -1)
   {
      if(errno != EAGAIN && errno != EWOULDBLOCK)
      {
         // TODO: Error Handling
         fprintf(stderr, "Error durring accept: %s\n", strerror(errno));
      }
      return NULL;
   }

   (void)ryannet_set_nonblocking(new_fd); 
   (void)ryannet_set_tcp_nodelay(new_fd);

   new_socket = ryannet_socket_tcp_new();
   new_socket->fd = new_fd;

   // Copy Remote
   memcpy(&new_socket->remote.raw, &remote_address, length);
   ryannet_fill_address(&new_socket->remote);
   // Copy Local
   length = sizeof(struct sockaddr_storage);
   getsockname(new_socket->fd, (struct sockaddr *)&new_socket->local.raw, &length);
   ryannet_fill_address(&new_socket->local);

   new_socket->connected_flag = 1;
   return new_socket;
}

int ryannet_socket_tcp_receive(struct ryannet_socket_tcp * socket, void * buffer, int buffer_size_in_bytes)
{
   int bytes_received;

   bytes_received = recv(socket->fd, buffer, (size_t)buffer_size_in_bytes, 0);
   if(bytes_received == 0)
   {
      socket->remote_closed_flag = 1;
   }
   if(bytes_received == -1)
   {
      if(errno == EAGAIN || errno == EWOULDBLOCK)
      {
         bytes_received = 0;
      }
      else if(errno == ECONNREFUSED)
      {
         socket->remote_closed_flag = 1;
      }
      else
      {
         // TODO: Error Handling
      }
     
   }
   return bytes_received;
}

int ryannet_socket_tcp_send(struct ryannet_socket_tcp * socket, const void * buffer, int buffer_size_in_bytes)
{
   int bytes_sent;
   int rv;
   fd_set wfds;
   struct timeval timeout;


   bytes_sent = (int)send(socket->fd, buffer, (size_t)buffer_size_in_bytes, 0);
   if(bytes_sent == -1)
   {
      if(errno == EAGAIN || errno == EWOULDBLOCK)
      {
         // We need to wait a bit for the buffer to be created 
         FD_ZERO(&wfds);
         FD_SET(socket->fd, &wfds);
         timeout.tv_sec = 0;
         timeout.tv_usec = 10000;
         rv = select(socket->fd + 1, NULL, &wfds, NULL, &timeout);
         if(rv > 0)
         {
            bytes_sent = (int)send(socket->fd, buffer, (size_t)buffer_size_in_bytes, 0);
            if(bytes_sent == -1)
            {
               if(errno == EAGAIN || errno == EWOULDBLOCK)
               {
                  bytes_sent = 0;
               }
               else if(errno == ECONNRESET)
               {
                  socket->remote_closed_flag = 1;
               }
               else
               {
                  // TODO: Handel Errors
               }

            }
           
           
         }
         else
         {
            // TODO: Handel Errors
         }

      }
      else if(errno == ECONNRESET)
      {
         socket->remote_closed_flag = 1;
      }
      else
      {
         // TODO: Handel Errors
      }
   }
   return bytes_sent;
}


struct ryannet_address * ryannet_socket_tcp_get_address_local(struct ryannet_socket_tcp * socket)
{
   return &socket->local;
}

struct ryannet_address * ryannet_socket_tcp_get_address_remote(struct ryannet_socket_tcp * socket)
{
   return &socket->remote;
}

int ryannet_socket_tcp_is_connected(struct ryannet_socket_tcp * socket)
{
   int rv;
   if(socket->connected_flag == 1 && 
      socket->remote_closed_flag == 0 &&
      socket->fd != -1)
   {
      rv = 1;
   }
   else
   {
      rv = 0;
   }
   return rv;
}


