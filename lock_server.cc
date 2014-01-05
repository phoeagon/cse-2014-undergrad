// the lock server implementation

#include "lock_server.h"
#include <sstream>
#include <stdio.h>
#include <unistd.h>
#include <arpa/inet.h>

lock_server::lock_server():
  nacquire (0)
{
	if (pthread_mutex_init(&mut, NULL)) {
		printf("error init pthread mutex");
		assert(false);
	}
	if (pthread_cond_init(&cond, NULL)) {
		printf("error init pthread condition variable");
		assert(false);
	}
	return;
}

lock_protocol::status
lock_server::stat(int clt, lock_protocol::lockid_t lid, int &r)
{
  lock_protocol::status ret = lock_protocol::OK;
  printf("stat request from clt %d\n", clt);
  r = nacquire;
  return ret;
}

lock_protocol::status
lock_server::acquire(int clt, lock_protocol::lockid_t lid, int &r)
{
	r = lock_protocol::OK;
	pthread_mutex_lock(&mut);
	while (lock_state[lid] != lock_protocol::FREE) {
		pthread_cond_wait(&cond, &mut);
	}
	lock_state[lid] = lock_protocol::LOCKED;
	pthread_mutex_unlock(&mut);
	return 0;
}

lock_protocol::status
lock_server::release(int clt, lock_protocol::lockid_t lid, int &r)
{
	std::map<lock_protocol::lockid_t, lock_protocol::lkstatus>::iterator it;
	r = lock_protocol::RPCERR;
	pthread_mutex_lock(&mut);
	it = lock_state.find(lid);

	// try to unlock a non-existing lock
	if (it == lock_state.end()) {
		pthread_mutex_unlock(&mut);
		assert(false);
		return 0;
	}
	// try to unlock a already freed lock
	if (it->second == lock_protocol::LOCKED) {
		r = lock_protocol::OK;
		it->second = lock_protocol::FREE;
		pthread_cond_broadcast(&cond);
	} else {
		assert(false);
	}
	pthread_mutex_unlock(&mut);
	return 0;
}

