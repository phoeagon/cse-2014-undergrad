// extent client interface.

#ifndef extent_client_h
#define extent_client_h

#include <string>
#include <map>
#include "extent_protocol.h"
#include "extent_server.h"
#include "lock_client_cache.h"

class buffer_cache_entry {
public:
  bool dirty;
  bool deleted;
  std::string buf;

  buffer_cache_entry();
};


class extent_client {
 private:
  rpcc *cl;
  std::map<extent_protocol::extentid_t, extent_protocol::attr> acache;
  std::map<extent_protocol::extentid_t, buffer_cache_entry> bcache;
  pthread_mutex_t mut;

 public:
  extent_client(std::string dst);

  extent_protocol::status create(uint32_t type, extent_protocol::extentid_t &eid);
  extent_protocol::status get(extent_protocol::extentid_t eid, 
			                        std::string &buf);
  extent_protocol::status getattr(extent_protocol::extentid_t eid, 
				                          extent_protocol::attr &a);
  extent_protocol::status put(extent_protocol::extentid_t eid, std::string buf);
  extent_protocol::status remove(extent_protocol::extentid_t eid);

  void flush(extent_protocol::extentid_t eid);
};


class lock_release_user_client : public lock_release_user {
private:
  extent_client *ec;
public:
  virtual void dorelease(lock_protocol::lockid_t);
  lock_release_user_client(extent_client *);
};

#endif

