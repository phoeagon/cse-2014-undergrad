#ifndef lock_server_cache_h
#define lock_server_cache_h

#include <string>
#include <map>
#include <list>
#include "lock_protocol.h"
#include "rpc.h"
#include "lock_server.h"

struct client_state {
public:
  std::string id;
  bool send_rpc;

  client_state(const std::string &_id);
  client_state();
};

struct lock_state_server {
public:
	lock_protocol::lkstatus state;
	client_state holding_client; 
  std::list<std::string> waiting_clients;

	lock_state_server();
};


class lock_server_cache {
 private:
  int nacquire;
  std::map<lock_protocol::lockid_t, lock_state_server> lock_states_server;
  pthread_mutex_t mut;

 public:
  lock_server_cache();
  lock_protocol::status stat(lock_protocol::lockid_t, int &);
  int acquire(lock_protocol::lockid_t, std::string id, int &);
  int release(lock_protocol::lockid_t, std::string id, int &);
};

#endif
