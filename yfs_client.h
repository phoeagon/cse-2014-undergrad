#ifndef yfs_client_h
#define yfs_client_h

#include <string>
//#include "yfs_protocol.h"
#include "extent_client.h"
#include <vector>
#include <map>

#define YFS_SET_ATTR_SIZE  0x1
class yfs_client {
  extent_client *ec;
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
  static std::string filename(inum);
  static inum n2i(std::string);

 public:

  yfs_client();
  bool isfile(inum);
  bool isdir(inum);
  int getfile(inum, fileinfo &);
  int getdir(inum, dirinfo &);
  int create(inum parent, const std::string name,inum &ino);
  int readdir(inum ino,std::map<inum,std::string> &items);
  int setattr(inum ino,int to_set,const fileinfo &info);
  int read(inum ino,size_t size,off_t off,std::string &buf);
  int write(inum ino,const char* buf,size_t &size,off_t off);

  inum lookup(inum parent, const std::string name);
  int mkdir(inum parent, const std::string name,inum &ino);
  int unlink(inum parent, const std::string name);
};
struct inode
{
  //fill this in lab1
};
#endif 
