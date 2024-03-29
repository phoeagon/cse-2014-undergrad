#ifndef yfs_client_h
#define yfs_client_h

#include <string>

#include "lock_protocol.h"
#include "lock_client.h"
#include "lock_client_cache.h"

//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>
#include <set>


class yfs_client {
  extent_client *ec;
  lock_release_user *lu;
  //lock_client *lc;
  lock_client_cache *lc;
 public:

  typedef unsigned long long inum;
  enum xxstatus { OK, RPCERR, NOENT, IOERR, EXIST };
  typedef int status;

  struct fileinfo {
    unsigned long long size;
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirinfo {
    unsigned long atime;
    unsigned long mtime;
    unsigned long ctime;
  };
  struct dirent {
    std::string name;
    yfs_client::inum inum;
  };

 private:
  std::set<inum> held_lock;
  static std::string filename(inum);
  static inum n2i(std::string);
  static char *parsedir(char *p, int &remain, uint32_t &inum,  uint32_t &filelen, uint32_t &entrylen, std::string &name);
  static void newdir(std::string &content);
  static char *filldir(int &size,
    uint32_t inum, uint32_t newnamelen, uint32_t entrylen, const char *name, uint32_t preventrylen);

  int lock(inum);
  int unlock(inum);

 public:
  yfs_client(std::string, std::string);

  bool isfile(inum);
  bool isdir(inum);

  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);

  int setattr(inum, size_t);
  int lookup(inum, const char *, bool &, inum &);
  int create(inum, const char *, mode_t, inum &, int type);
  int readdir(inum, std::list<dirent> &);
  int write(inum, size_t, off_t, const char *, size_t &);
  int read(inum, size_t, off_t, std::string &);
  int unlink(inum,const char *);
};

#endif
