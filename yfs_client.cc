// yfs client.  implements FS operations using extent and lock server
#include "yfs_client.h"
#include "extent_client.h"
#include <sstream>
#include <iostream>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

yfs_client::yfs_client(std::string extent_dst, std::string lock_dst)
{
  int lr;
  std::string content;
  ec = new extent_client(extent_dst);
  lc = new lock_client(lock_dst);
  newdir(content);
  lr = lock((inum) 1);
  if (ec->put(1, content) != extent_protocol::OK)
      printf("error init root dir\n"); // XYB: init root dir
  if (!lr) unlock((inum) 1);
}

void
yfs_client::newdir(std::string &content) {
    char buf[16];
    // create a dummy entry
    *((uint32_t *) buf) = 0;
    *((uint32_t *) (buf + 4)) = 0;
    *((uint32_t *) (buf + 8)) = 12;
    content.assign(buf, 12);
    return;
}

char *
yfs_client::filldir(int &size, uint32_t inum, uint32_t newnamelen, uint32_t entrylen, const char *name, uint32_t preventrylen) {
    char *cstr;
    int offset = 0;
    if (preventrylen) {
        cstr = new char[16 + newnamelen];
        *((uint32_t *) cstr) = preventrylen;
        offset = 4;
    } else {
        cstr = new char[12 + newnamelen];
    }

    *((uint32_t *) (cstr + offset)) = inum;
    *((uint32_t *) (cstr + offset + 4)) = newnamelen;
    if (newnamelen) 
        memcpy(cstr + offset + 8, name, newnamelen);
    *((uint32_t *) (cstr + offset + newnamelen + 8)) = entrylen;

    size = 12 + newnamelen + offset;

    return cstr;
}

yfs_client::inum
yfs_client::n2i(std::string n)
{
    std::istringstream ist(n);
    unsigned long long finum;
    ist >> finum;
    return finum;
}

std::string
yfs_client::filename(inum inum)
{
    std::ostringstream ost;
    ost << inum;
    return ost.str();
}

char *
yfs_client::parsedir(char *p, int &remain, 
    uint32_t &inum, uint32_t &filelen, uint32_t &entrylen, std::string &name) {
    if (remain < 8) {
        assert(remain == 0);
        return NULL;
    }
    inum = *((uint32_t *) p);
    filelen = *((uint32_t *) (p + 4));
    if (remain < (int)(filelen + 12)) {
        assert(0);
    }
    if (filelen)
        name.assign(p + 8, filelen);
    else
        name = "";
    entrylen = *((uint32_t *) (p + 8 + filelen));
    remain -= entrylen;
    return (p + entrylen);
}

int
yfs_client::lock(inum i) {
    if (!held_lock.count(i)) {
        VERIFY(!(lc->acquire((lock_protocol::lockid_t) i)));
        held_lock.insert(i);
        return 0;
    }
    return 1;
}

int
yfs_client::unlock(inum i) {
    std::set<inum>::iterator it = held_lock.find(i);
    VERIFY(it != held_lock.end());
    held_lock.erase(it);
    VERIFY(!(lc->release((lock_protocol::lockid_t) i)));
    return 0;
}

bool
yfs_client::isfile(inum inum)
{
    int lr;
    extent_protocol::attr a;

    lr = lock(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        if (!lr) unlock(inum);
        printf("error getting attr\n");
        return false;
    }
    if (!lr) unlock(inum);

    if (a.type == extent_protocol::T_FILE) {
        printf("isfile: %lld is a file\n", inum);
        return true;
    } 
    printf("isfile: %lld is a dir\n", inum);
    return false;
}

bool
yfs_client::isdir(inum inum)
{
    return ! isfile(inum);
}

int
yfs_client::getfile(inum inum, fileinfo &fin)
{
    int lr;
    int r = OK;

    printf("getfile %016llx\n", inum);
    extent_protocol::attr a;
    lr = lock(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        if (!lr) unlock(inum);
        r = IOERR;
        goto release;
    }
    if (!lr) unlock(inum);

    fin.atime = a.atime;
    fin.mtime = a.mtime;
    fin.ctime = a.ctime;
    fin.size = a.size;
    printf("getfile %016llx -> sz %llu\n", inum, fin.size);

release:
    return r;
}

int
yfs_client::getdir(inum inum, dirinfo &din)
{
    int lr;
    int r = OK;

    printf("getdir %016llx\n", inum);
    extent_protocol::attr a;
    lr = lock(inum);
    if (ec->getattr(inum, a) != extent_protocol::OK) {
        if (!lr) unlock(inum);
        r = IOERR;
        goto release;
    }
    if (!lr) unlock(inum);
    din.atime = a.atime;
    din.mtime = a.mtime;
    din.ctime = a.ctime;

release:
    return r;
}


#define EXT_RPC(xx) do { \
    if ((xx) != extent_protocol::OK) { \
        printf("EXT_RPC Error: %s:%d \n", __FILE__, __LINE__); \
        r = IOERR; \
        goto release; \
    } \
} while (0)

// Only support set size of attr
int
yfs_client::setattr(inum ino, size_t size)
{
    int lr;
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: get the content of inode ino, and modify its content
     * according to the size (<, =, or >) content length.
     */
    std::string buf;
    lr = lock(ino);
    if ((r = ec->get(ino, buf)) != OK) {
        if (!lr) unlock(ino);
        return r; 
    }

    if (size != buf.size()) {
        buf.resize(size, '\0');
    }

    //TODO might need to modify time
    r = ec->put(ino, buf);
    if (!lr) unlock(ino);

    return r;
}

int
yfs_client::create(inum parent, const char *name, mode_t mode, inum &ino_out, int type)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: lookup is what you need to check if file exist;
     * after create file or dir, you must remember to modify the parent infomation.
     */
    inum ino;
    bool found;

    char *cstr, *pbuf, *nbuf, *ncstr;
    uint32_t inum, namelen, entrylen;
    std::string fname;
    int remain;
    int newnamelen;
    int nsize;
    size_t bytes_written;
    std::string buf;

    int lr = lock(parent);

    if ((r = lookup(parent, name, found, ino)) != OK) {
        if (!lr) unlock(parent);
        return r;
    }

    if (found) {
        if (!lr) unlock(parent);
        return EXIST;
    }
    if ((r = ec->create(type, ino)) != OK) {
        if (!lr) unlock(parent);
        return r;
    }
    std::string content;
    if (type == extent_protocol::T_DIR) {
        newdir(content);
        if ((r = ec->put(ino, content) != extent_protocol::OK) != OK) {
            printf("create dir failed\n");
            if (!lr) unlock(parent);
            return r;
        }

    }
    ino_out = ino;


    if ((r = ec->get(parent, buf)) != OK) {
        if (!lr) unlock(parent);
        return r;
    }
    remain = buf.length();
    cstr = new char[remain + 1];
    memcpy(cstr, buf.c_str(), remain);
    newnamelen = strlen(name);

    pbuf = cstr;
    
    while ((nbuf = parsedir(pbuf, remain, inum, namelen, entrylen, fname))) {
        if (newnamelen + namelen + 24 < entrylen) {
            //inject an entry
            ncstr = filldir(nsize, ino, newnamelen, entrylen - namelen - 12, name, namelen + 12);
            r = write(parent, nsize, pbuf - cstr - 4, ncstr, bytes_written);
            goto free_create;
        }
        pbuf = nbuf;
    }

    //append an entry
    ncstr = filldir(nsize, ino, newnamelen, newnamelen + 12,name, 0);
    r = write(parent, nsize, pbuf - cstr, ncstr, bytes_written);

free_create:
    if (!lr) unlock(parent);
    delete[] cstr;
    delete[] ncstr;
    return r;
}

int
yfs_client::lookup(inum parent, const char *name, bool &found, inum &ino_out)
{
    int r = OK;


    /*
     * your lab2 code goes here.
     * note: lookup file from parent dir according to name;
     * you should design the format of directory content.
     */
    std::list<dirent> entries;

    found = false;
    if ((r = readdir(parent, entries)) != OK)
        return r;
    for (std::list<dirent>::iterator it = entries.begin(); it != entries.end(); ++it) {
        if (!it->name.compare(name)) {
            found = true;
            ino_out = it->inum;
            break;
        }
    }
    return r;
}

int
yfs_client::readdir(inum dir, std::list<dirent> &list)
{
    int lr;
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should parse the dirctory content using your defined format,
     * and push the dirents to the list.
     */
    char *cstr, *pbuf, *nbuf;
    uint32_t inum, namelen, entrylen;
    std::string fname;
    int remain;
    dirent dentry;
    std::string buf;
    list.clear();
    lr = lock(dir);
    if ((r = ec->get(dir, buf)) != OK) {
        if (!lr) unlock(dir);
        return r;
    }
    if (!lr) unlock(dir);
    remain = buf.length();
    cstr = new char[remain + 1];
    memcpy(cstr, buf.c_str(), remain);

    pbuf = cstr;
    while ((nbuf = parsedir(pbuf, remain, inum, namelen, entrylen, fname))) {
        if (inum) {
            dentry.inum = inum;
            dentry.name = fname;
            list.push_back(dentry);
            printf("readdir: entry(%u, %s)\n", inum, fname.c_str());
        }
        pbuf = nbuf;
    }

    delete[] cstr;
    return r;
}

int
yfs_client::read(inum ino, size_t size, off_t off, std::string &data)
{
    int lr;
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: read using ec->get().
     */

    std::string buf;
    lr = lock(ino);
    if ((r = ec->get(ino, buf)) != OK) {
        if (!lr) unlock(ino);
        return r;
    }
    if (!lr) unlock(ino);
    data = buf.substr(off, size);

    return r;
}

int
yfs_client::write(inum ino, size_t size, off_t off, const char *data,
        size_t &bytes_written)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: write using ec->put().
     * when off > length of original file, fill the holes with '\0'.
     */
     int lr;
    std::string buf;
    int len;
    lr = lock(ino);
    if ((r = ec->get(ino, buf)) != OK) {
        if (!lr) unlock(ino);
        return r;
    }
    len = buf.size();
    if (off + size > buf.size())
        buf.resize(off + size, '\0');

    buf = buf.replace(off, size, data, size);
    bytes_written = off >= len ? off + size - len : size;
    r = ec->put(ino, buf);
    if (!lr) unlock(ino);
    return r;
}

int yfs_client::unlink(inum parent,const char *name)
{
    int r = OK;

    /*
     * your lab2 code goes here.
     * note: you should remove the file using ec->remove,
     * and update the parent directory content.
     */

     int lr1, lr2;
    
    bool found;
    inum ino;

    char *cstr, *pbuf, *nbuf, *ncstr = NULL;
    uint32_t inum, namelen, entrylen;
    std::string fname;
    int remain, nsize;
    size_t bytes_written;
    std::string buf;

    if ((r = lookup(parent, name, found, ino)) != OK)
        return r;

    if (!found)
        return NOENT;

    if (parent < ino) {
        lr1 = lock(parent);
        lr2 = lock(ino);
    } else {
        lr2 = lock(ino);
        lr1 = lock(parent);
    }

    if (isdir(ino)) {
        if (!lr2) unlock(ino);
        if (!lr1) unlock(parent);
        return ENOTEMPTY;
    }

    if ((r = ec->get(parent, buf)) != extent_protocol::OK) {
        if (!lr2) unlock(ino);
        if (!lr1) unlock(parent);
        return r;
    }

    remain = buf.length();
    cstr = new char[remain + 1];
    memcpy(cstr, buf.c_str(), remain);

    pbuf = cstr;
    found = false;
    while ((nbuf = parsedir(pbuf, remain, inum, namelen, entrylen, fname))) {
        if (inum && !fname.compare(name)) {
            if (inum == ino)
                found = true;
            break;
        }
        pbuf = nbuf;
    }

    if (found) {
        ncstr = filldir(nsize, 0, 0, entrylen, "", 0);
        r = write(parent, nsize, pbuf - cstr, ncstr, bytes_written);
    }
    if (!lr1) unlock(parent);
    delete[] cstr;

    r = ec->remove(ino);
    if (!lr2) unlock(ino);
    return r;
}
