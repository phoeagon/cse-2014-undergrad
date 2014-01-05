// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>

void
lock_release_user_client::dorelease(lock_protocol::lockid_t lid) {
  printf("lock_release: DO RELEASE %llu\n", lid);
  ec->flush(lid);
}

lock_release_user_client::lock_release_user_client(extent_client * _ec)
  : ec(_ec)
{
}

buffer_cache_entry::buffer_cache_entry()
  : dirty(false), deleted(false)
{
}

extent_client::extent_client(std::string dst)
{
  sockaddr_in dstsock;
  make_sockaddr(dst.c_str(), &dstsock);
  VERIFY(pthread_mutex_init(&mut, NULL) == 0);
  cl = new rpcc(dstsock);
  if (cl->bind() != 0) {
    printf("extent_client: bind failed\n");
  }
}

// a demo to show how to use RPC
extent_protocol::status
extent_client::getattr(extent_protocol::extentid_t eid,
		       extent_protocol::attr &attr)
{
  extent_protocol::status ret = extent_protocol::OK;
  printf("extent_client: GETATTR %llu\n", eid);
  pthread_mutex_lock(&mut);
  if (acache.find(eid) != acache.end()) {
    attr = acache[eid];
    printf("extent_client: GETATTR FROM CACHE %llu, %u\n", eid, attr.size);
    pthread_mutex_unlock(&mut);
    return extent_protocol::OK;
  }
  pthread_mutex_unlock(&mut);

  printf("extent_client: GETATTR FROM SERVER %llu\n", eid);

  ret = cl->call(extent_protocol::getattr, eid, attr);
  if (ret == extent_protocol::OK) {
    pthread_mutex_lock(&mut);
    acache[eid] = attr;
    pthread_mutex_unlock(&mut);
  }
  return ret;
}

extent_protocol::status
extent_client::create(uint32_t type, extent_protocol::extentid_t &id)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  ret = cl->call(extent_protocol::create, type, id);
  return ret;
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, std::string &buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  printf("extent_client: GET: %llu\n", eid);
  pthread_mutex_lock(&mut);
  if (bcache.find(eid) != bcache.end()) {
    buf = bcache[eid].buf;
    printf("extent_client: GET FROM CACHE: %llu, (%s)[%lu]\n", eid, buf.c_str(), buf.size());
    pthread_mutex_unlock(&mut);
    return extent_protocol::OK;
  }
  pthread_mutex_unlock(&mut);
  printf("extent_client: GET FROM SERVER: %llu\n", eid);

  ret = cl->call(extent_protocol::get, eid, buf);
  if (ret == extent_protocol::OK) {
    pthread_mutex_lock(&mut);
    bcache[eid].buf = buf;
    bcache[eid].dirty = false;
    pthread_mutex_unlock(&mut);
  }
  return ret;
}

extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, std::string buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  /*int r;
  ret = cl->call(extent_protocol::put, eid, buf, r);*/
  printf("extent_client: PUT TO CACHE: %llu, (%s)[%lu]\n", eid, buf.c_str(), buf.size());
  pthread_mutex_lock(&mut);
  acache[eid].size = buf.size();
  bcache[eid].buf = buf;
  bcache[eid].dirty = true;
  pthread_mutex_unlock(&mut);

  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  // Your lab3 code goes here
  printf("extent_client: REMOVE FROM CACHE: %llu\n", eid);
  pthread_mutex_lock(&mut);
  bcache[eid].deleted = true;
  //bcache.erase(eid);
  //acache.erase(eid);
  pthread_mutex_unlock(&mut);
  /*int r;
  ret = cl->call(extent_protocol::remove, eid, r);

  if (ret == extent_protocol::OK) {
    pthread_mutex_lock(&mut);
    acache.erase(eid);
    bcache.erase(eid);
    pthread_mutex_unlock(&mut);
  }*/
  return ret;
}

void
extent_client::flush(extent_protocol::extentid_t eid)
{
  int r;
  extent_protocol::status ret = extent_protocol::OK;
  buffer_cache_entry bce;
  pthread_mutex_lock(&mut);
  if (bcache.find(eid) == bcache.end()) {
    printf("extent_client: CACHE NOT FOUND");
    pthread_mutex_unlock(&mut);
  } else {
    bce = bcache[eid];
    bcache.erase(eid);
    acache.erase(eid);

    pthread_mutex_unlock(&mut);

    printf("extent_client: CACHE FOUND %llu, dirty: %s, deleted: %s\n",
      eid, bce.dirty ? "true" : "false", bce.deleted ? "true": "false");

    if (bce.deleted) {
      ret = cl->call(extent_protocol::remove, eid, r);
      printf("extent_client: REMOVE %llu\n", eid);
      assert(ret == extent_protocol::OK);
    } else if (bce.dirty) {
      ret = cl->call(extent_protocol::put, eid, bce.buf, r);
      printf("extent_client: PUT TO SERVER %llu, %s[%lu]\n", eid, bce.buf.c_str(), bce.buf.size());
      assert(ret == extent_protocol::OK);
    }
  }
}


