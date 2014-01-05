// the caching lock server implementation

#include "lock_server_cache.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "lang/verify.h"
#include "handle.h"
#include "tprintf.h"

client_state::client_state()
  :id(""), send_rpc(false)
{
}

client_state::client_state(const std::string &_id)
  : id(_id), send_rpc(false)
{
}

lock_state_server::lock_state_server()
	: state(lock_protocol::FREE)
{
}


lock_server_cache::lock_server_cache()
{
	if (pthread_mutex_init(&mut, NULL)) {
		printf("error init pthread mutex");
		assert(false);
	}
}

//TODO NEED TO GUARANTEE there each client in the waiting_clients is UNIQUE?

//TODO send_rpc has 2 meanings under FREE and EXPECTING states

int lock_server_cache::acquire(lock_protocol::lockid_t lid, std::string id,
                               int &)
{
  int rr, r;
  lock_protocol::status ret = lock_protocol::OK;
  std::string dest_client_id;
  bool need_to_revoke = false;

  pthread_mutex_lock(&mut);
  //tprintf("lock server cache: acquire [%llu] from %s\n", lid, id.c_str());
  if (lock_states_server[lid].state == lock_protocol::FREE) {
    assert(lock_states_server[lid].waiting_clients.empty());
	  lock_states_server[lid].state = lock_protocol::LOCKED;
	  lock_states_server[lid].holding_client.id = id;
    lock_states_server[lid].holding_client.send_rpc = false;
    //tprintf("lock server cache: acquire [%llu] from %s: FREE->LOCKED\n", lid, id.c_str());
  	pthread_mutex_unlock(&mut);
  } else {
    assert(lock_states_server[lid].holding_client.id != "");
/* CAN CLIENT GUARANTEE sending acquire ahead of sending release?
    CAN. */
    if (lock_states_server[lid].state == lock_protocol::EXPECTING) {
     // tprintf("lock server cache: acquire [%llu] from %s: EXPECTING\n", lid, id.c_str());
        if (lock_states_server[lid].holding_client.id == id) {
          lock_states_server[lid].state = lock_protocol::LOCKED;
          if (lock_states_server[lid].holding_client.send_rpc) {
            need_to_revoke = true;
            dest_client_id = lock_states_server[lid].holding_client.id;
          }
        } else {
/*
client A send acquire when server is expecting acquire from another client B
*/
          lock_states_server[lid].waiting_clients.push_back(id);
          lock_states_server[lid].holding_client.send_rpc = true;
          ret = lock_protocol::RETRY;
        }
    } else if (lock_states_server[lid].state == lock_protocol::LOCKED) {
    	  assert(lock_states_server[lid].holding_client.id != id);
    	  lock_states_server[lid].waiting_clients.push_back(id);

        ret = lock_protocol::RETRY;
        if (lock_states_server[lid].holding_client.send_rpc == false) {
          dest_client_id = lock_states_server[lid].holding_client.id;
          lock_states_server[lid].holding_client.send_rpc = true;
          need_to_revoke = true;
         // tprintf("lock server cache: acquire [%llu] from %s: RETRY & sending revoke to %s\n",
            //lid, id.c_str(), dest_client_id.c_str());
        } else {
        //  tprintf("lock server cache: acquire [%llu] from %s: RETRY\n", lid, id.c_str());
        }
    } else {
      assert(false);
    }
  	pthread_mutex_unlock(&mut);

//TODO this range might contain race
// #####################
    if (need_to_revoke) {
      // tprintf("lock server cache: sending revoke [%llu] to %s\n", lid, dest_client_id.c_str());
	     handle h(dest_client_id);
	     rpcc *clrpc = h.safebind();
	     assert(clrpc != NULL);
	     r = clrpc->call(rlock_protocol::revoke, lid, rr);
      // tprintf("lock server cache: sending revoke [%llu] to %s: COMPLETE\n", lid, dest_client_id.c_str());
       assert(r == rlock_protocol::OK);
    }
// #####################
  }

  return ret;
}


//TODO QUESTIONS:
// When to update lock_states_server::holding_client & lock_states_server::waiting_clients
// Lock Server need to guarantee the order of locks acquired

int
lock_server_cache::release(lock_protocol::lockid_t lid, std::string id,
         int &r)
{
  int rr;
  lock_protocol::status ret = lock_protocol::OK;
  std::string dest_client_id;

  pthread_mutex_lock(&mut);
 // tprintf("lock server cache: release [%llu] from %s\n", lid, id.c_str());
  assert(lock_states_server[lid].state == lock_protocol::LOCKED);
  assert(lock_states_server[lid].holding_client.id == id);
  if (lock_states_server[lid].waiting_clients.empty()) {
	  lock_states_server[lid].holding_client.id = "";
    lock_states_server[lid].holding_client.send_rpc = false;
	  lock_states_server[lid].state = lock_protocol::FREE;
    //tprintf("lock server cache: release [%llu] from %s: LOCKED -> FREE\n", lid, id.c_str());
    pthread_mutex_unlock(&mut);
  } else {
//TODO need to diretly let the waiting one holding lock?
//TODO currently answer is adding a new state named EXPECTING
//TODO set field"need_rpc" for EXPECING
	  dest_client_id = lock_states_server[lid].waiting_clients.front();
    lock_states_server[lid].waiting_clients.pop_front();
	  lock_states_server[lid].holding_client.id = dest_client_id;
    lock_states_server[lid].holding_client.send_rpc = !(lock_states_server[lid].waiting_clients.empty());
    lock_states_server[lid].state = lock_protocol::EXPECTING;
    //tprintf("lock server cache: release [%llu] from %s: LOCKED -> FREE: set %s to EXPECTING\n",
    //  lid, id.c_str(), dest_client_id.c_str());
  	pthread_mutex_unlock(&mut);
//TODO this range might contain race
// #####################
	  handle h(dest_client_id);
	  rpcc *clrpc = h.safebind();
	  assert(clrpc != NULL);
	  clrpc->call(rlock_protocol::retry, lid, rr);
// #####################
  }
  return ret;
}

lock_protocol::status
lock_server_cache::stat(lock_protocol::lockid_t lid, int &r)
{
  tprintf("stat request\n");
  r = nacquire;
  return lock_protocol::OK;
}

