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
#include <poll.h>
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

struct ryannet_socket_udp
{
   int fd;
   struct ryannet_address local;
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


#define PORT_STRING_SIZE 8
static void ryannet_fill_address(struct ryannet_address * address)
{
   size_t size_address, size_port;
   int rv;

   // Address
   if(address->address != NULL)
   {
      free(address->address);
      address->address = NULL;
   }
   switch(address->raw.ss_family)
   {
   case AF_INET:
      size_address = INET_ADDRSTRLEN;
      break;
   case AF_INET6:
      size_address = INET6_ADDRSTRLEN;
      break;
   default:
      size_address = 0;
      break;
   }

   if(size_address > 0)
   {
      address->address = malloc(sizeof(char) * size_address);
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
      size_port = PORT_STRING_SIZE;
      break;
   case AF_INET6:
      size_port = PORT_STRING_SIZE;
      break;
   default:
      size_port = 0;
      break;
   }
   if(size_port > 0)
   {
      address->port = malloc(sizeof(char) * size_port);
   }

   rv = getnameinfo((struct sockaddr *)&address->raw, sizeof(struct sockaddr_storage),
                    address->address, size_address,
                    address->port, size_port, NI_NUMERICHOST | NI_NUMERICSERV);

   if(rv != 0)
   {
      fprintf(stderr, "Error getnameinfo: %s\n", gai_strerror(rv));
      if(address->address != NULL)
      {
         free(address->address);
      }
      if(address->port != NULL)
      {
         free(address->port);
      }
   }

}

static void ryannet_getaddrinfo_dump(const char * node, const char * port)
{

   struct addrinfo hints, *servinfo, *p;
   struct ryannet_address * address;
   int rv;

   address = ryannet_address_new();

   memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_family = AF_UNSPEC;
   hints.ai_socktype = SOCK_STREAM;
   hints.ai_flags = AI_PASSIVE; // Use any local IPV4 or IPV6 address I have
   
   rv = getaddrinfo(node, port, &hints, &servinfo);
   if(rv != 0)
   {
      fprintf(stderr, "getaddrinfo %s\n", gai_strerror(rv));
      return;
   }

   for(p = servinfo; p != NULL; p = p->ai_next)
   {
      memcpy(&address->raw, p->ai_addr, p->ai_addrlen);
      ryannet_fill_address(address);
      printf("getaddrinfo: %s : %s\n", address->address, address->port);

   }
   freeaddrinfo(servinfo);
   ryannet_address_destroy(address);

}

struct ryannet_address * ryannet_address_new(void)
{
   struct ryannet_address * addy;
   addy = malloc(sizeof(struct ryannet_address));
   addy->address = NULL;
   addy->port = NULL;
   memset(&addy->raw, 0, sizeof(struct sockaddr_storage));
   return addy;
}

int ryannet_address_set(struct ryannet_address * address, const char * node, const char * port)
{
   struct addrinfo hints, *servinfo, *p;
   int rv;
   socklen_t length;

   memset(&hints, 0, sizeof(struct addrinfo));
   hints.ai_family = AF_UNSPEC;
   
   rv = getaddrinfo(node, port, &hints, &servinfo);
   if(rv != 0)
   {
      fprintf(stderr, "getaddrinfo %s\n", gai_strerror(rv));
      return 1;
   }

   memcpy(&address->raw, &servinfo->ai_addr, servinfo->ai_addrlen);
   ryannet_fill_address(address);

   freeaddrinfo(servinfo);
   return 0;
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


static int ryannet_set_tcp_nodelay(int fd)
{
   int rv;
#ifdef _WIN32
   char yes = TRUE;
   int length = sizeof(BOOL);
#else // _WIN32
   int yes = 1;
   socklen_t length = sizeof(int);
#endif // _WIN32
   if(setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, length) == -1)
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
#ifdef _WIN32
   char yes = TRUE;
   int length = sizeof(BOOL);
#else // _WIN32
   int yes = 1;
   socklen_t length = sizeof(int);
#endif // _WIN32
   if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, length) == -1)
   {
      rv = 1;
   }
   else
   {
      rv = 0;
   }
   return rv;
}

static int ryannet_clear_v6only(int fd, int family)
{
   int rv;
#ifdef _WIN32
   char no = FALSE;
   int length = sizeof(BOOL);
#else // _WIN32
   int no = 0;
   socklen_t length = sizeof(int);
#endif // _WIN32
   if(family == AF_INET6)
   {
      if(setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, length) == -1)
      {
         rv = 1;
      }
      else
      {
         rv = 0;
      }
   }
   else
   {
      rv = 0;
   }

   return rv;
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

   //ryannet_getaddrinfo_dump(bind_address, bind_port);

   memset(&hints, 0, sizeof(struct addrinfo));
   if(bind_address == NULL)
   {
      // Windows and linux have differing opinons on what to do here, so
      // we will take the choice away
      hints.ai_family = AF_INET;
   }
   else
   {
      hints.ai_family = AF_UNSPEC;
   }
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


      rv = ryannet_set_reuseaddr(sock->fd);
      if(rv == 1)
      {
         // TODO: Maybe Error Handling
         ryannet_close(sock->fd);
         sock->fd = -1;
         continue;
      }

      rv = ryannet_clear_v6only(sock->fd, p->ai_family);
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

   if(sock->fd != -1)
   {
      rv = listen(sock->fd, 10);
      if(rv == -1)
      {
         ryannet_close(sock->fd);
         sock->fd = -1;
         // TODO: Error Handling
      }
   }


   if(sock->fd == -1)
   {
      // TODO: Error Handing
      fprintf(stderr, "Error: Couldn't Bind to %s : %s\n", bind_address, bind_port);
      return 1;
   }
   
   // Copy Local
   length = sizeof(struct sockaddr_storage);
   getsockname(sock->fd, (struct sockaddr *)&sock->local.raw, &length);
   ryannet_fill_address(&sock->local);

   sock->connected_flag = 1;
   freeaddrinfo(servinfo);

   return 0;
   
}

struct ryannet_socket_tcp * ryannet_socket_tcp_accept(struct ryannet_socket_tcp * socket)
{
   struct ryannet_socket_tcp * new_socket;
   socklen_t length;
   length = sizeof(struct sockaddr_storage);
   new_socket = ryannet_socket_tcp_new();
   new_socket->fd = accept(socket->fd, (struct sockaddr *)&new_socket->remote.raw, &length);
   if(new_socket->fd == -1)
   {
      // TODO: Error Handling
      fprintf(stderr, "Error durring accept: %s\n", strerror(errno));
      ryannet_socket_tcp_destroy(new_socket);
      return NULL;
   }

   (void)ryannet_set_tcp_nodelay(new_socket->fd);

   // Copy Remote
   ryannet_fill_address(&new_socket->remote);

   // Copy Local
   length = sizeof(struct sockaddr_storage);
   getsockname(new_socket->fd, (struct sockaddr *)&new_socket->local.raw, &length);
   ryannet_fill_address(&new_socket->local);

   new_socket->connected_flag = 1;
   return new_socket;
}

struct ryannet_socket_tcp * ryannet_socket_tcp_accept_nonblock(struct ryannet_socket_tcp * socket)
{
   struct ryannet_socket_tcp * new_socket;
   int rv;
#ifdef _WIN32
   WSAPOLLFD fds;
   fds.fd = socket->fd;
   fds.events = POLLRDNORM;

   rv = WSAPoll(&fds, 1, 0);
#else // _WIN32
   struct pollfd fds;
   fds.fd = socket->fd;
   fds.events = POLLIN;

   rv = poll(&fds, 1, 0);
#endif // _WIN32
   if(rv > 0)
   {
      new_socket = ryannet_socket_tcp_accept(socket);
   }
   else if(rv < 0)
   {
      // Error Here
      fprintf(stderr, "polling Error: %s\n", strerror(errno));
      new_socket = NULL;
   }
   else
   {
      new_socket = NULL;
   }

      

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
   else if(bytes_received == -1)
   {
      fprintf(stderr, "Error durring receive: %s\n", strerror(errno));
   }
   return bytes_received;
}

int ryannet_socket_tcp_receive_nonblock(struct ryannet_socket_tcp * socket, void * buffer, int buffer_size_in_bytes)
{
   int bytes_sent;
   int rv;
#ifdef _WIN32
   WSAPOLLFD fds;
   fds.fd = socket->fd;
   fds.events = POLLRDNORM;

   rv = WSAPoll(&fds, 1, 0);
#else // _WIN32
   struct pollfd fds;
   fds.fd = socket->fd;
   fds.events = POLLIN;

   rv = poll(&fds, 1, 0);
#endif // _WIN32
   if(rv > 0)
   {
      bytes_sent = ryannet_socket_tcp_receive(socket, buffer, buffer_size_in_bytes);
   }
   else if(rv < 0)
   {
      // Error
      bytes_sent = -1;
      fprintf(stderr, "Error durring poll: %s\n", strerror(errno));
   }
   else
   {
      bytes_sent = 0;
   }
   return bytes_sent;

}

int ryannet_socket_tcp_send(struct ryannet_socket_tcp * socket, const void * buffer, int buffer_size_in_bytes)
{
   int bytes_sent;

   bytes_sent = (int)send(socket->fd, buffer, (size_t)buffer_size_in_bytes, 0);
   if(bytes_sent == -1)
   {
      fprintf(stderr, "Error durring send: %s\n", strerror(errno));
   }
   return bytes_sent;
}


struct ryannet_address * ryannet_socket_tcp_get_address_local(struct ryannet_socket_tcp * socket)
{
   if(socket->fd == -1)
   {
      return NULL;
   }
   return &socket->local;
   
}

struct ryannet_address * ryannet_socket_tcp_get_address_remote(struct ryannet_socket_tcp * socket)
{
   if(socket->fd == -1)
   {
      return NULL;
   }
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

struct ryannet_socket_udp * ryannet_socket_udp_new(void)
{
   struct ryannet_socket_udp * socket;
   socket = malloc(sizeof(struct ryannet_socket_udp));
   socket->fd = -1;
   socket->local.address = NULL;
   socket->local.port = NULL;
   memset(&socket->local.raw, 0, sizeof(struct sockaddr_storage));

   return socket;
}

void ryannet_socket_udp_destroy(struct ryannet_socket_udp * socket)
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
   free(socket);
}

int ryannet_socket_udp_bind(struct ryannet_socket_udp * sock, const char * bind_address, const char * bind_port)
{
   struct addrinfo hints, *servinfo, *p;
   int rv;
   socklen_t length;

   memset(&hints, 0, sizeof(struct addrinfo));
   if(bind_address == NULL)
   {
      // Windows and linux have differing opinons on what to do here, so
      // we will take the choice away
      hints.ai_family = AF_INET;
   }
   else
   {
      hints.ai_family = AF_UNSPEC;
   }
   hints.ai_socktype = SOCK_DGRAM;
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

      rv = ryannet_clear_v6only(sock->fd, p->ai_family);
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


   if(sock->fd == -1)
   {
      // TODO: Error Handing
      fprintf(stderr, "Error: Couldn't Bind to %s : %s\n", bind_address, bind_port);
      return 1;
   }

   // Copy Local
   length = sizeof(struct sockaddr_storage);
   getsockname(sock->fd, (struct sockaddr *)&sock->local.raw, &length);
   ryannet_fill_address(&sock->local);

   freeaddrinfo(servinfo);

   return 0;
}

int ryannet_socket_udp_send(struct ryannet_socket_udp * sock, struct ryannet_address * destination, const void * buffer, int buffer_size_in_bytes)
{
   int sent_bytes;
   if(sock->fd == -1)
   {
      struct addrinfo hints, *servinfo, *p;
      int rv;
      socklen_t length;

      memset(&hints, 0, sizeof(struct addrinfo));
      hints.ai_family = AF_UNSPEC;
      hints.ai_socktype = SOCK_DGRAM;
      
      rv = getaddrinfo(destination->address, destination->port, &hints, &servinfo);
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
         break;
      }

      if(sock->fd == -1)
      {
         // TODO: Error Handing
         fprintf(stderr, "Error: Couldn't Create Socket to %s : %s\n", destination->address, destination->port);
         return 1;
      }

      // Copy Local
      length = sizeof(struct sockaddr_storage);
      getsockname(sock->fd, (struct sockaddr *)&sock->local.raw, &length);
      ryannet_fill_address(&sock->local);
   }
   if(sock->fd != -1)
   {
      sent_bytes = sendto(sock->fd, buffer, buffer_size_in_bytes, 0, (struct sockaddr *) &destination->raw, sizeof(struct sockaddr_storage));
   }
   else
   {
      sent_bytes = 0;
   }
   return sent_bytes;
}

int ryannet_socket_udp_receive(struct ryannet_socket_udp * socket, void * buffer, int buffer_size_in_bytes, struct ryannet_address * source)
{
   int received_bytes;
   socklen_t length;

   length = sizeof(struct sockaddr_storage);
   if(socket->fd != -1)
   {
      received_bytes = recvfrom(socket->fd, buffer, buffer_size_in_bytes, 0, (struct sockaddr *)&source->raw, &length);
   }
   else
   {
      received_bytes = 0;
   }
   return received_bytes;
}
int ryannet_socket_udp_receive_nonblock(struct ryannet_socket_udp * socket, void * buffer, int buffer_size_in_bytes, struct ryannet_address * source)
{
   int received_bytes;
   int rv;
#ifdef _WIN32
   WSAPOLLFD fds;
   fds.fd = socket->fd;
   fds.events = POLLRDNORM;

   rv = WSAPoll(&fds, 1, 0);
#else // _WIN32
   struct pollfd fds;
   fds.fd = socket->fd;
   fds.events = POLLIN;

   rv = poll(&fds, 1, 0);
#endif // _WIN32
   if(rv > 0)
   {
      received_bytes = ryannet_socket_udp_receive(socket, buffer,  buffer_size_in_bytes, source);
   }
   else if(rv < 0)
   {
      // Error
      received_bytes = -1;
      fprintf(stderr, "Error durring poll: %s\n", strerror(errno));
   }
   else
   {
      received_bytes = 0;
   }
   return received_bytes;
}

struct ryannet_address * ryannet_socket_udp_get_address_local(struct ryannet_socket_udp * socket)
{
   if(socket->fd == -1)
   {
      return NULL;
   }
   return &socket->local;
}

