// RPC stubs for clients to talk to extent_server

#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/stat.h>

extent_client::extent_client()
{
}

extent_protocol::status
extent_client::get(extent_protocol::extentid_t eid, char* buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  //fill this for lab1
  return ret;
}


extent_protocol::status
extent_client::put(extent_protocol::extentid_t eid, char* buf)
{
  extent_protocol::status ret = extent_protocol::OK;
  //fill this for lab1
  return ret;
}

extent_protocol::status
extent_client::remove(extent_protocol::extentid_t eid)
{
  extent_protocol::status ret = extent_protocol::OK;
  //fill this lab1
  return ret;
}


