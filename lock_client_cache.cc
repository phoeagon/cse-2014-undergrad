// RPC stubs for clients to talk to lock_server, and cache the locks
// see lock_client.cache.h for protocol details.

#include "lock_client_cache.h"
#include "rpc.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include "tprintf.h"

#define lsc (lock_states_client[lid])


lock_state_client::lock_state_client()
  : state(rlock_protocol::NONE), pending(0), locked(false)
{
}

int lock_client_cache::last_port = 0;

void lock_client_cache::_acquire(lock_protocol::lockid_t lid) {
  //tprintf("TRY LOCKING: %llu AT %s(%lu)\n", lid, id.c_str(), pthread_self());
  pthread_mutex_lock(&mut);
  //tprintf("LOCKED: %llu AT %s(%lu)\n", lid, id.c_str(), pthread_self());
//  while (lock_states_client[lid].locked)
//    pthread_cond_wait(&cond, &mut);
//  lock_states_client[lid].locked = true;
//  pthread_mutex_unlock(&mut);
}

void lock_client_cache::_release(lock_protocol::lockid_t lid, bool broadcast) {
//  pthread_mutex_lock(&mut);
//  lock_states_client[lid].locked = false;
//  pthread_cond_broadcast(&cond);
  //tprintf("TRY UNLOCKING: %llu AT %s(%lu)\n", lid, id.c_str(), pthread_self());
  if (broadcast) {
    //tprintf("BROADCAST: %llu AT %s(%lu)\n", lid, id.c_str(), pthread_self());
    pthread_cond_broadcast(&cond);
  }
  pthread_mutex_unlock(&mut);
  //tprintf("UNLOCKED: %llu AT %s(%lu)\n", lid, id.c_str(), pthread_self());
}

//guarantee is holding the lock before invoking _rpc_call
lock_protocol::status lock_client_cache::_rpc_call(lock_protocol::rpc_numbers method,
  lock_protocol::lockid_t lid) {
  int rr;
  lock_protocol::status r;
  //tprintf("BEFORE RPC: %llu AT %s(%lu)\n", lid, id.c_str(), pthread_self());

//  pthread_mutex_lock(&mut);
//  lock_states_client[lid].locked = false;
//  pthread_cond_broadcast(&cond);
  _release(lid);
  //pthread_mutex_unlock(&mut);

  r = cl->call(method, lid, id, rr);

  _acquire(lid);

  //pthread_mutex_lock(&mut);
//  while (lock_states_client[lid].locked)
//    pthread_cond_wait(&cond, &mut);
//  lock_states_client[lid].locked = true;
//  pthread_cond_unlock(&mut);

  return r;
}

lock_client_cache::lock_client_cache(std::string xdst, 
				     class lock_release_user *_lu)
  : lock_client(xdst), lu(_lu)
{
  srand(time(NULL)^last_port);
  rlock_port = ((rand()%32000) | (0x1 << 10));
  const char *hname;
  // VERIFY(gethostname(hname, 100) == 0);
  hname = "127.0.0.1";
  std::ostringstream host;
  host << hname << ":" << rlock_port;
  id = host.str();
  
  if (pthread_mutex_init(&mut, NULL)) {
    printf("error init pthread mutex client");
    assert(false);
  }

  if (pthread_cond_init(&cond, NULL)) {
    printf("error init pthread condition variable");
    assert(false);
  }

  last_port = rlock_port;
  rpcs *rlsrpc = new rpcs(rlock_port);
  rlsrpc->reg(rlock_protocol::revoke, this, &lock_client_cache::revoke_handler);
  rlsrpc->reg(rlock_protocol::retry, this, &lock_client_cache::retry_handler);
}

lock_protocol::status
lock_client_cache::acquire(lock_protocol::lockid_t lid)
{
  int r;//, rr;

/*  pthread_mutex_lock(&mut);
  while (lock_states_client[lid].locked)
      pthread_cond_wait(&cond, &mut);
  lock_states_client[lid].locked = true;
  pthread_mutex_unlock(&mut);

  tprintf("lock_client_cache: acquiring [%llu] from %s(%lu)\n", lid, id.c_str(), pthread_self());
  if (lock_states_client[lid].state == rlock_protocol::OWNED) {
    assert(lock_states_client[lid].pending != rlock_protocol::retry);
    tprintf("lock_client_cache: acquiring [%llu] from %s(%lu) OWNED->LOCKED\n", lid, id.c_str(), pthread_self());
    lock_states_client[lid].state = rlock_protocol::LOCKED;
  } else {
    while (lock_states_client[lid].state != rlock_protocol::OWNED) {
      if (lock_states_client[lid].state == rlock_protocol::NONE) {
        assert(lock_states_client[lid].pending != lock_protocol::RETRY);
        r = cl->call(lock_protocol::acquire, lid, id, rr);
        if (r == lock_protocol::OK) {
          tprintf("lock_client_cache: acquiring [%llu] from %s(%lu) NONE->OWNED\n", lid, id.c_str(), pthread_self());
          lock_states_client[lid].state = rlock_protocol::OWNED;
        } else if (r == lock_protocol::RETRY) {
          tprintf("lock_client_cache: acquiring [%llu] from %s(%lu) NONE->SLEEP\n", lid, id.c_str(), pthread_self());
          pthread_mutex_lock(&mut);
          lock_states_client[lid].locked = false;
          do {
            pthread_cond_wait(&cond, &mut);
          } while (lock_states_client[lid].locked);
          lock_states_client[lid].locked = true;
          pthread_mutex_unlock(&mut);
          tprintf("lock_client_cache: acquiring [%llu] from %s(%lu) NONE->SLEEP->?\n", lid, id.c_str(), pthread_self());
        } else {
          assert(false);
        }
      } else if (lock_states_client[lid].state == rlock_protocol::LOCKED) {
        assert(lock_states_client[lid].pending != lock_protocol::RETRY);
        pthread_mutex_lock(&mut);
        lock_states_client[lid].locked = false;
        tprintf("lock_client_cache: acquiring [%llu] from %s(%lu) LOCKED->SLEEP\n", lid, id.c_str(), pthread_self());
        do {
          pthread_cond_wait(&cond, &mut);
        } while (lock_states_client[lid].locked);
        lock_states_client[lid].locked = true;
        pthread_mutex_unlock(&mut);
        tprintf("lock_client_cache: acquiring [%llu] from %s(%lu) LOCKED->SLEEP->?\n", lid, id.c_str(), pthread_self());
      } else {
        assert(false);
      }
    }
    tprintf("lock_client_cache: acquiring [%llu] from %s(%lu) OWNED->LOCKED1\n", lid, id.c_str(), pthread_self());
    lock_states_client[lid].state = rlock_protocol::LOCKED;
  }

  pthread_mutex_lock(&mut);
  tprintf("lock client cache: acquire [%llu] AT %s(%lu)\n", lid, id.c_str(), pthread_self());
  lock_states_client[lid].locked = false;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&mut);*/
 // tprintf("lock_client_cache: before acquiring [%llu] from %s(%lu)\n", lid, id.c_str(), pthread_self());
  _acquire(lid);
 // lock_state_client &lsc = lock_states_client[lid];
  //tprintf("lock_client_cache: acquiring [%llu] from %s(%lu)\n", lid, id.c_str(), pthread_self());

  if (lsc.state == rlock_protocol::OWNED) {
    lsc.state = rlock_protocol::LOCKED;
    //tprintf("lock_client_cache: acquiring [%llu] from %s(%lu) OWNED->LOCKED\n", lid, id.c_str(), pthread_self());
  } else {
    do {
    if (lsc.state == rlock_protocol::NONE) {
      lsc.state = rlock_protocol::ACQUIRING;
      //tprintf("lock_client_cache: acquiring [%llu] from %s(%lu) NONE->ACQUIRING\n", lid, id.c_str(), pthread_self());
      r = _rpc_call(lock_protocol::acquire, lid);
      assert(lsc.state == rlock_protocol::ACQUIRING);
      if (r == lock_protocol::OK) {
        lsc.state = rlock_protocol::LOCKED;
        //tprintf("lock_client_cache: acquiring [%llu] from %s(%lu) NONE->ACQUIRING->LOCKED\n", lid, id.c_str(), pthread_self());
        goto finish_acquire;
      } else {
        lsc.state = rlock_protocol::WAITING;
        //tprintf("lock_client_cache: acquiring [%llu] from %s(%lu) NONE->ACQUIRING->WAITING\n", lid, id.c_str(), pthread_self());
        assert(lsc.pending != rlock_protocol::revoke);
        if (lsc.pending == rlock_protocol::retry) {
          lsc.pending = 0;
          r = _rpc_call(lock_protocol::acquire, lid);
          assert(r == lock_protocol::OK);
          lsc.state = rlock_protocol::LOCKED;
          goto finish_acquire;
        }
      }
    }

    //tprintf("lock_client_cache: acquiring [%llu] from %s(%lu) SLEEPING\n", lid, id.c_str(), pthread_self());
     pthread_cond_wait(&cond, &mut);

    /*while (lsc.state != rlock_protocol::OWNED) {
      if (lsc.state == rlock_protocol.NONE) {

      }
      pthread_cond_wait(&cond, &mut);
    }*/
  } while (lsc.state != rlock_protocol::OWNED);
  lsc.state = rlock_protocol::LOCKED;
  }

finish_acquire:
  _release(lid);

  return lock_protocol::OK;
}

lock_protocol::status
lock_client_cache::release(lock_protocol::lockid_t lid)
{
  int r;//, rr;
  /*pthread_mutex_lock(&mut);
  while (lock_states_client[lid].locked)
      pthread_cond_wait(&cond, &mut);
  lock_states_client[lid].locked = true;
  pthread_mutex_unlock(&mut);
  tprintf("lock client cache: releasing [%llu] AT %s(%lu)\n", lid, id.c_str(), pthread_self());

  assert(lock_states_client[lid].state != rlock_protocol::NONE);
  assert(lock_states_client[lid].state != rlock_protocol::OWNED);
  assert(lock_states_client[lid].pending != rlock_protocol::retry);

  if (lock_states_client[lid].pending == rlock_protocol::revoke) {
    tprintf("lock client cache: releasing1 [%llu] AT %s(%lu) LOCKED->NONE\n", lid, id.c_str(), pthread_self());
    r = cl->call(lock_protocol::release, lid, id, rr);
    tprintf("lock client cache: releasing1-1 [%llu] AT %s(%lu) LOCKED->NONE\n", lid, id.c_str(), pthread_self());
    assert(r == lock_protocol::OK);
    lock_states_client[lid].pending = 0;
    lock_states_client[lid].state = rlock_protocol::NONE;
  } else if (lock_states_client[lid].pending == 0) {
    tprintf("lock client cache: releasing2 [%llu] AT %s(%lu) LOCKED->OWNED\n", lid, id.c_str(), pthread_self());
    lock_states_client[lid].state = rlock_protocol::OWNED;
  } else {
    assert(false);
  }

  pthread_mutex_lock(&mut);
  tprintf("lock client cache: release [%llu] AT %s(%lu)\n", lid, id.c_str(), pthread_self());
  lock_states_client[lid].locked = false;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&mut);*/
 //tprintf("lock_client_cache: before releasing [%llu] from %s(%lu)\n", lid, id.c_str(), pthread_self());
  _acquire(lid);
  //tprintf("lock_client_cache: releasing [%llu] from %s(%lu)\n", lid, id.c_str(), pthread_self());
//  lock_state_client &lsc = lock_states_client[lid];
  bool need_to_wakeup = false;

  assert(lsc.state == rlock_protocol::LOCKED);
  assert(lsc.pending != rlock_protocol::retry);

  if (lsc.pending == 0) {
   // tprintf("lock_client_cache: releasing [%llu] from %s(%lu) LOCKED->OWNED\n", lid, id.c_str(), pthread_self());
    lsc.state = rlock_protocol::OWNED;
    need_to_wakeup = true;
  } else {
    //tprintf("lock_client_cache: releasing [%llu] from %s(%lu) LOCKED->RELEASING\n", lid, id.c_str(), pthread_self());
    lsc.state = rlock_protocol::RELEASING;
    lsc.pending = 0;
    r = _rpc_call(lock_protocol::release, lid);
    assert(r == rlock_protocol::OK);
    assert(lsc.state == rlock_protocol::RELEASING);
    //tprintf("lock_client_cache: releasing [%llu] from %s(%lu) LOCKED->RELEASING->NONE\n", lid, id.c_str(), pthread_self());
    lsc.state = rlock_protocol::NONE;
  }

  _release(lid, need_to_wakeup);

  return lock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::revoke_handler(lock_protocol::lockid_t lid, 
                                  int &)
{
  int r;//, rr;
  /*int ret = rlock_protocol::OK;
  tprintf("lock client cache: revoke0 [%llu] AT %s(%lu)\n", lid, id.c_str(), pthread_self());
  pthread_mutex_lock(&mut);
  tprintf("lock client cache: revoke0 [%llu] AT %s(%lu): LOCK ACQUIRE\n", lid, id.c_str(), pthread_self());
  while (lock_states_client[lid].locked)
    pthread_cond_wait(&cond, &mut);
  lock_states_client[lid].locked = true;
  pthread_mutex_unlock(&mut);

  tprintf("lock client cache: revoke [%llu] AT %s(%lu)\n", lid, id.c_str(), pthread_self());
  assert(lock_states_client[lid].pending == 0);

  if (lock_states_client[lid].state == rlock_protocol::NONE) {
    lock_states_client[lid].pending = rlock_protocol::revoke;
    tprintf("lock client cache: revoke-1 [%llu] AT %s(%lu)\n", lid, id.c_str(), pthread_self());
  } else if (lock_states_client[lid].state == rlock_protocol::OWNED) {
    tprintf("lock client cache: revoke-2-0 [%llu] AT %s(%lu)\n", lid, id.c_str(), pthread_self());
    r = cl->call(lock_protocol::release, lid, id, rr);
    assert(r == lock_protocol::OK);
    tprintf("lock client cache: revoke-2-1 [%llu] AT %s(%lu)\n", lid, id.c_str(), pthread_self());
    lock_states_client[lid].pending = 0;
    lock_states_client[lid].state = rlock_protocol::NONE;
  } else if (lock_states_client[lid].state == rlock_protocol::LOCKED) {
    tprintf("lock client cache: revoke-2-0 [%llu] AT %s(%lu)\n", lid, id.c_str(), pthread_self());
    lock_states_client[lid].pending = rlock_protocol::revoke;
  } else {
    assert(false);
  }

  pthread_mutex_lock(&mut);
  lock_states_client[lid].locked = false;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&mut);*/
 // tprintf("lock_client_cache: before revoking [%llu] from %s(%lu)\n", lid, id.c_str(), pthread_self());
  _acquire(lid);
  bool need_to_wakeup = false;
//  lock_state_client &lsc = lock_states_client[lid];

  assert(lsc.state != rlock_protocol::NONE);
  if (lsc.pending != 0) {
   // tprintf("lock_client_cache: pending = %d\n", lsc.pending);
    assert(lsc.pending == 0);
  }

  if (lsc.state == rlock_protocol::OWNED) {
    lsc.state = rlock_protocol::RELEASING;
   // tprintf("lock_client_cache: revoking [%llu] from %s(%lu) RELEASE\n", lid, id.c_str(), pthread_self());
    r = _rpc_call(lock_protocol::release, lid);
    assert(r == lock_protocol::OK);
    lsc.state = rlock_protocol::NONE;
    need_to_wakeup = true;
    //tprintf("lock_client_cache: revoking [%llu] from %s(%lu) OWNED->NONE\n", lid, id.c_str(), pthread_self());
  } else {
    //tprintf("lock_client_cache: revoking [%llu] from %s(%lu) PENDING->REVOKE\n", lid, id.c_str(), pthread_self());
    lsc.pending = rlock_protocol::revoke;
  }
  
  _release(lid, need_to_wakeup);
  return rlock_protocol::OK;
}

rlock_protocol::status
lock_client_cache::retry_handler(lock_protocol::lockid_t lid, 
                                 int &)
{
  int r;//, rr;
  /*int ret = rlock_protocol::OK;

  pthread_mutex_lock(&mut);
  while (lock_states_client[lid].locked)
    pthread_cond_wait(&cond, &mut);
  lock_states_client[lid].locked = true;
  pthread_mutex_unlock(&mut);
  assert(lock_states_client[lid].pending == 0);
  assert(lock_states_client[lid].state == rlock_protocol::NONE);

  tprintf("lock_client_cache: retry [%llu] AT %s\n", lid, id.c_str());
  r = cl->call(lock_protocol::acquire, lid, id, rr);
  assert(r == lock_protocol::OK);
  tprintf("lock_client_cache: retry [%llu] AT %s: ACQUIRED\n", lid, id.c_str());

  pthread_mutex_lock(&mut);
  lock_states_client[lid].state = rlock_protocol::OWNED;
  lock_states_client[lid].locked = false;
  pthread_cond_broadcast(&cond);
  pthread_mutex_unlock(&mut);
  */
 // tprintf("lock_client_cache: before retrying [%llu] from %s(%lu)\n", lid, id.c_str(), pthread_self());
  _acquire(lid);
  //tprintf("lock_client_cache: retrying [%llu] from %s(%lu)\n", lid, id.c_str(), pthread_self());
  bool need_to_wakeup = false;

  assert(lsc.pending == 0);
  assert(lsc.state == rlock_protocol::ACQUIRING ||
    lsc.state == rlock_protocol::WAITING);

  if (lsc.state == rlock_protocol::ACQUIRING) {
    lsc.pending = rlock_protocol::retry;
    //tprintf("lock_client_cache: retrying [%llu] from %s(%lu) PENDING->RETRY\n", lid, id.c_str(), pthread_self());
  } else {
   // tprintf("lock_client_cache: retrying [%llu] from %s(%lu) RE-ACQUIRE\n", lid, id.c_str(), pthread_self());
    r = _rpc_call(lock_protocol::acquire, lid);
    assert(r == lock_protocol::OK);
   // tprintf("lock_client_cache: retrying [%llu] from %s(%lu) WAITING->OWNED\n", lid, id.c_str(), pthread_self());
    lsc.state = rlock_protocol::OWNED;
    need_to_wakeup = true;
  }

  _release(lid, need_to_wakeup);

  return rlock_protocol::OK;
}