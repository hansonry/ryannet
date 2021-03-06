#ifndef __RYANNET_H__
#define __RYANNET_H__

struct ryannet_address;
struct ryannet_socket_tcp;
struct ryannet_socket_udp;

int ryannet_init(void);
void ryannet_destroy(void);

struct ryannet_address * ryannet_address_new(void);
int ryannet_address_set(struct ryannet_address * address, const char * node, const char * port);
void ryannet_address_destroy(struct ryannet_address * address);

const char * ryannet_address_get_address(struct ryannet_address * address);
const char * ryannet_address_get_port(struct ryannet_address * address);

struct ryannet_socket_tcp * ryannet_socket_tcp_new(void);
void ryannet_socket_tcp_destroy(struct ryannet_socket_tcp * socket);

int ryannet_socket_tcp_connect(struct ryannet_socket_tcp * socket, const char * remote_address, const char * remote_port);

int ryannet_socket_tcp_bind(struct ryannet_socket_tcp * socket, const char * bind_address, const char * bind_port);

struct ryannet_socket_tcp * ryannet_socket_tcp_accept(struct ryannet_socket_tcp * socket);
struct ryannet_socket_tcp * ryannet_socket_tcp_accept_nonblock(struct ryannet_socket_tcp * socket);


int ryannet_socket_tcp_receive(struct ryannet_socket_tcp * socket, void * buffer, int buffer_size_in_bytes);
int ryannet_socket_tcp_receive_nonblock(struct ryannet_socket_tcp * socket, void * buffer, int buffer_size_in_bytes);
int ryannet_socket_tcp_send(struct ryannet_socket_tcp * socket, const void * buffer, int buffer_size_in_bytes);

struct ryannet_address * ryannet_socket_tcp_get_address_local(struct ryannet_socket_tcp * socket);
struct ryannet_address * ryannet_socket_tcp_get_address_remote(struct ryannet_socket_tcp * socket);

int ryannet_socket_tcp_is_connected(struct ryannet_socket_tcp * socket);



struct ryannet_socket_udp * ryannet_socket_udp_new(void);
void ryannet_socket_udp_destroy(struct ryannet_socket_udp * socket);

int ryannet_socket_udp_bind(struct ryannet_socket_udp * socket, const char * bind_address, const char * bind_port);

int ryannet_socket_udp_send(struct ryannet_socket_udp * socket, struct ryannet_address * destination, const void * buffer, int buffer_size_in_bytes);
int ryannet_socket_udp_receive(struct ryannet_socket_udp * socket, void * buffer, int buffer_size_in_bytes, struct ryannet_address * source);
int ryannet_socket_udp_receive_nonblock(struct ryannet_socket_udp * socket, void * buffer, int buffer_size_in_bytes, struct ryannet_address * source);

struct ryannet_address * ryannet_socket_udp_get_address_local(struct ryannet_socket_udp * socket);

#endif // __RYANNET_H__


