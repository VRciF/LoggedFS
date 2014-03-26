
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <utime.h>
#include <sys/time.h>
#include <errno.h>

#include <sys/types.h>
#include <attr/xattr.h>

#include <sys/file.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE             /* See feature_test_macros(7) */
#endif

#include <fcntl.h>

#include <fuse.h>

#include <sstream>

#include "Config.h"
#include "FSOperations.h"

#include "Util.h"

std::map<int, std::string> FSOperations::actions;

LogMap& operator<<(LogMap &what, std::pair<Format::Specifier,char *> kv){
	what.values.insert(kv);
	return what;
}
LogMap& operator<<(LogMap &what, std::pair<Format::Specifier,const char *> kv){
	what.values.insert(kv);
	return what;
}
LogMap& operator<<(LogMap &what, std::pair<Format::Specifier,std::string> kv)
{
	what.values.insert(kv);
    return what;
}
LogMap& operator<<(LogMap &what, std::pair<Format::Specifier,long int> kv)
{
	what.values.insert(std::make_pair(kv.first, ((std::stringstream&)(std::stringstream() << kv.second)).str()));
    return what;
}

void* FSOperations::init(struct fuse_conn_info* info)
{
	 ::fchdir(Globals::instance()->savefd);
	 ::close(Globals::instance()->savefd);

	if(FSOperations::actions.size()<=0){

		FSOperations::actions[OP_GETATTR]="getattr";
		FSOperations::actions[OP_FGETATTR]="fgetattr";
		FSOperations::actions[OP_ACCESS]="access";
		FSOperations::actions[OP_READLINK]="readlink";
		FSOperations::actions[OP_OPENDIR]="opendir";
		FSOperations::actions[OP_READDIR]="readdir";
		FSOperations::actions[OP_RELEASEDIR]="releasedir";
		FSOperations::actions[OP_MKNOD]="mknod";
		FSOperations::actions[OP_MKDIR]="mkdir";
		FSOperations::actions[OP_SYMLINK]="symlink";
		FSOperations::actions[OP_UNLINK]="unlink";
		FSOperations::actions[OP_RMDIR]="rmdir";
		FSOperations::actions[OP_RENAME]="rename";
		FSOperations::actions[OP_LINK]="link";
		FSOperations::actions[OP_CHMOD]="chmod";
		FSOperations::actions[OP_CHOWN]="chown";
		FSOperations::actions[OP_FTRUNCATE]="ftruncate";
		FSOperations::actions[OP_TRUNCATE]="truncate";
		//FSOperations::actions[OP_UTIME]="utime";  // deprecated
		FSOperations::actions[OP_UTIMENS]="utimens";
		FSOperations::actions[OP_READ_BUF]="read_buf";
		FSOperations::actions[OP_WRITE_BUF]="write_buf";
		FSOperations::actions[OP_FLUSH]="flush";
		FSOperations::actions[OP_CREATE]="create";
		FSOperations::actions[OP_OPEN]="open";
		FSOperations::actions[OP_READ]="read";
		FSOperations::actions[OP_WRITE]="write";
		FSOperations::actions[OP_STATFS]="statfs";
		FSOperations::actions[OP_RELEASE]="release";
		FSOperations::actions[OP_FSYNC]="fsync";
		FSOperations::actions[OP_FALLOCATE]="fallocate";
		FSOperations::actions[OP_SETXATTR]="setxattr";
		FSOperations::actions[OP_GETXATTR]="getxattr";
		FSOperations::actions[OP_LISTXATTR]="listxattr";
		FSOperations::actions[OP_REMOVEXATTR]="removexattr";
		FSOperations::actions[OP_LOCK]="lock";
		FSOperations::actions[OP_FLOCK]="flock";

		FSOperations::actions[OP_BMAP]="bmap";
		FSOperations::actions[OP_IOCTL]="ioctl";
		FSOperations::actions[OP_POLL]="poll";
	}

	 return NULL;
}

template <int N>
int FSOperations::proxy(const char *path, ...){
	int result = 0;

    va_list ap;
    va_start(ap, path); /* Requires the last fixed parameter (to get the address) */

	LogMap lm;
	if(Globals::instance()->config.fuzzyShouldLog(N)){
		lm << std::make_pair(Format::ACTION, FSOperations::actions[N])
		   << std::make_pair(Format::RELPATH, path);
	}

    switch(N){
        case FSOperations::OP_FGETATTR:
        {
    	    struct stat *stbuf = (struct stat *)va_arg(ap, void *);
    	    struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
    	    result = FSOperations::fgetattr(path, stbuf, fi);
        }
    	break;
        case FSOperations::OP_GETATTR:
        {
        	struct stat *stbuf = (struct stat *)va_arg(ap, void *);
        	result = FSOperations::getattr(path, stbuf);
        }
        break;
        case FSOperations::OP_ACCESS:
        {
        	int mask = va_arg(ap, int);
        	result = FSOperations::access(path, mask);
        }
        break;
        case FSOperations::OP_READLINK:
        {
        	char *buf = va_arg(ap, char *);
        	size_t size = va_arg(ap, size_t);
        	result = FSOperations::readlink(path, buf, size);
        }
        break;
        case FSOperations::OP_OPENDIR:
        {
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::opendir(path, fi);
        }
        break;
        case FSOperations::OP_READDIR:
        {
        	void *buf = va_arg(ap, void*);
        	fuse_fill_dir_t filler = (fuse_fill_dir_t)va_arg(ap, void*);
        	off_t offset = va_arg(ap, off_t);
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::readdir(path, buf, filler, offset, fi);
        }
        break;
        case FSOperations::OP_RELEASEDIR:
        {
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::releasedir(path, fi);
        }
        break;
        case FSOperations::OP_MKNOD:
        {
        	mode_t mode = va_arg(ap, mode_t);
        	dev_t rdev= va_arg(ap, dev_t);
        	result = FSOperations::mknod(path, mode, rdev);

			lm << std::make_pair(Format::MODE, (long int)mode)
			   << std::make_pair(Format::RDEV, (long int)rdev);
        }
        break;
        case FSOperations::OP_MKDIR:
        {
        	mode_t mode = va_arg(ap, mode_t);
        	result = FSOperations::mkdir(path, mode);

        	lm << std::make_pair(Format::MODE, (long int)mode);
        }
        break;
        case FSOperations::OP_UNLINK:
        {
        	result = FSOperations::unlink(path);
        }
        break;
        case FSOperations::OP_RMDIR:
        {
        	result = FSOperations::rmdir(path);
        }
        break;
        case FSOperations::OP_SYMLINK:
        {
    	    const char *from = path;
    	    const char *to = va_arg(ap, const char *);
        	result = FSOperations::symlink(from, to);

        	lm << std::make_pair(Format::RELPATH, from)
			   << std::make_pair(Format::RELFROM, from)
			   << std::make_pair(Format::RELTO, to)
			   << std::make_pair(Format::ABSFROM, Util::getAbsolutePath(from))
			   << std::make_pair(Format::ABSTO, Util::getAbsolutePath(to));
        }
        break;
        case FSOperations::OP_RENAME:
        {
    	    const char *from = path;
    	    const char *to = va_arg(ap, const char *);
    	    result = FSOperations::rename(from, to);

			lm << std::make_pair(Format::RELPATH, from)
			   << std::make_pair(Format::RELFROM, from)
			   << std::make_pair(Format::RELTO, to)
			   << std::make_pair(Format::ABSFROM, Util::getAbsolutePath(from))
			   << std::make_pair(Format::ABSTO, Util::getAbsolutePath(to));
        }
        break;
        case FSOperations::OP_LINK:
        {
    	    const char *from = path;
    	    const char *to = va_arg(ap, const char *);
    	    result = FSOperations::link(from, to);

			lm << std::make_pair(Format::RELPATH, from)
			   << std::make_pair(Format::RELFROM, from)
			   << std::make_pair(Format::RELTO, to)
			   << std::make_pair(Format::ABSFROM, Util::getAbsolutePath(from))
			   << std::make_pair(Format::ABSTO, Util::getAbsolutePath(to));
        }
        break;
        case FSOperations::OP_CHMOD:
        {
        	mode_t mode = va_arg(ap, mode_t);
        	result = FSOperations::chmod(path, mode);

			lm << std::make_pair(Format::MODE, (long int)mode);
        }
        break;
        case FSOperations::OP_CHOWN:
        {
        	uid_t uid = va_arg(ap, uid_t);
        	gid_t gid = va_arg(ap, gid_t);
        	result = FSOperations::chown(path, uid, gid);

			lm << std::make_pair(Format::UID, (long int)uid)
			   << std::make_pair(Format::UID, (long int)gid);
        }
        break;
        case FSOperations::OP_FTRUNCATE:
        {
        	off_t size = va_arg(ap, off_t);
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::ftruncate(path, size, fi);

			lm << std::make_pair(Format::SIZE, (long int)size);
        }
        break;
        case FSOperations::OP_TRUNCATE:
        {
        	off_t size = va_arg(ap, off_t);
        	result = FSOperations::truncate(path, size);

			lm << std::make_pair(Format::SIZE, (long int)size);
        }
        break;
        case FSOperations::OP_UTIME:
        {
        	struct utimbuf *buf = (struct utimbuf*)va_arg(ap, void*);

        	struct timespec ts[2];
        	ts[0].tv_sec = buf->actime;
        	ts[0].tv_nsec = 0;
        	ts[1].tv_sec = buf->modtime;
        	ts[1].tv_nsec = 0;
        	result = FSOperations::utimens(path, ts);

			lm << std::make_pair(Format::MODSECONDS, (long int)ts[1].tv_sec)
		       << std::make_pair(Format::MODMICROSECONDS, (long int)ts[1].tv_nsec)
		       << std::make_pair(Format::ACSECONDS, (long int)ts[0].tv_sec)
		       << std::make_pair(Format::ACMICROSECONDS, (long int)ts[0].tv_nsec);
        }
		break;
        case FSOperations::OP_UTIMENS:
        {
        	const struct timespec *ts = (const struct timespec*)va_arg(ap, void*);
        	result = FSOperations::utimens(path, ts);

			lm << std::make_pair(Format::MODSECONDS, (long int)ts[1].tv_sec)
		       << std::make_pair(Format::MODMICROSECONDS, (long int)ts[1].tv_nsec)
		       << std::make_pair(Format::ACSECONDS, (long int)ts[0].tv_sec)
		       << std::make_pair(Format::ACMICROSECONDS, (long int)ts[0].tv_nsec);
        }
        break;
        case FSOperations::OP_CREATE:
        {
        	mode_t mode = va_arg(ap, mode_t);
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::create(path, mode, fi);

			lm << std::make_pair(Format::FLAGS, (long int)fi->flags);
        }
        break;
        case FSOperations::OP_OPEN:
        {
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::open(path, fi);

			lm << std::make_pair(Format::FLAGS, (long int)fi->flags);
        }
        break;
        case FSOperations::OP_READ:
        {
        	char *buf = va_arg(ap, char*);
        	size_t size = va_arg(ap, size_t);
        	off_t offset = va_arg(ap, off_t);
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::read(path, buf, size, offset, fi);

			lm << std::make_pair(Format::SIZE, (long int)size)
			   << std::make_pair(Format::OFFSET, (long int)offset);
        }
        break;
        case FSOperations::OP_WRITE:
        {
        	const char *buf = va_arg(ap, const char*);
        	size_t size = va_arg(ap, size_t);
        	off_t offset = va_arg(ap, off_t);
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::write(path, buf, size, offset, fi);

			lm << std::make_pair(Format::SIZE, (long int)size)
			   << std::make_pair(Format::OFFSET, (long int)offset);
        }
        break;
        case FSOperations::OP_READ_BUF:
        {
        	struct fuse_bufvec **bufp = (struct fuse_bufvec **)va_arg(ap, void*);
        	size_t size = va_arg(ap, size_t);
        	off_t offset = va_arg(ap, off_t);
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::read_buf(path, bufp, size, offset, fi);

			lm << std::make_pair(Format::SIZE, (long int)size)
			   << std::make_pair(Format::OFFSET, (long int)offset);
        }
        break;
        case FSOperations::OP_WRITE_BUF:
        {
        	struct fuse_bufvec *buf = (struct fuse_bufvec *)va_arg(ap, void*);
        	off_t offset = va_arg(ap, off_t);
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::write_buf(path, buf, offset, fi);

			lm << std::make_pair(Format::SIZE, (long int)fuse_buf_size(buf))
			   << std::make_pair(Format::OFFSET, (long int)offset);
        }
        break;
        case FSOperations::OP_FLUSH:
        {
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::flush(path, fi);
        }
        break;
        case FSOperations::OP_STATFS:
        {
    	    struct statvfs *stbuf = (struct statvfs *)va_arg(ap, void *);
        	result = FSOperations::statfs(path, stbuf);
        }
        break;
        case FSOperations::OP_RELEASE:
        {
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::release(path, fi);
        }
        break;
        case FSOperations::OP_FSYNC:
        {
        	int isdatasync = va_arg(ap, int);
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::fsync(path, isdatasync, fi);
        }
        break;
        case FSOperations::OP_FSYNCDIR:
        {
        	int isdatasync = va_arg(ap, int);
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::fsyncdir(path, isdatasync, fi);
        }
        break;
        case FSOperations::OP_SETXATTR:
        {
        	const char *name = va_arg(ap, const char*);
        	const char *value = va_arg(ap, const char*);
        	size_t size = va_arg(ap, size_t);
    	    int flags = va_arg(ap, int);
        	result = FSOperations::setxattr(path, name, value, size, flags);

			lm << std::make_pair(Format::XATTRNAME, name)
			   << std::make_pair(Format::XATTRVALUE, std::string(value,size))
			   << std::make_pair(Format::SIZE, (long int)size)
			   << std::make_pair(Format::FLAGS, (long int)flags);
        }
        break;
        case FSOperations::OP_GETXATTR:
        {
        	const char *name = va_arg(ap, const char*);
        	char *value = va_arg(ap, char*);
        	size_t size = va_arg(ap, size_t);
        	result = FSOperations::getxattr(path, name, value, size);

			lm << std::make_pair(Format::XATTRNAME, name)
			   << std::make_pair(Format::XATTRVALUE, std::string(value,size))
			   << std::make_pair(Format::SIZE, (long int)size);
        }
        break;
        case FSOperations::OP_LISTXATTR:
        {
       	    char *list = va_arg(ap, char*);
        	size_t size = va_arg(ap, size_t);
        	result = FSOperations::listxattr(path, list, size);

			lm << std::make_pair(Format::XATTRLIST, std::string(list, size))
			   << std::make_pair(Format::SIZE, (long int)size);
        }
        break;
        case FSOperations::OP_REMOVEXATTR:
        {
    	    const char *name = va_arg(ap, const char*);
        	result = FSOperations::removexattr(path, name);
        }
        break;
        //case FSOperations::OP_LOCK:
        //{
        	//struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	//int cmd = va_arg(ap, int);
        	//struct flock *lock = (struct flock*)va_arg(ap, void*);
        	//result = FSOperations:::lock(path, fi, cmd, lock);
        //}
        //break;
        case FSOperations::OP_FLOCK:
        {
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	int op = va_arg(ap, int);
        	result = FSOperations::flock(path, fi, op);

			lm << std::make_pair(Format::FLAGS, (long int)op);
        }
        break;
        case FSOperations::OP_FALLOCATE:
        {
        	int mode = va_arg(ap, int);
        	off_t offset = va_arg(ap, off_t);
        	off_t length = va_arg(ap, off_t);
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	result = FSOperations::fallocate(path, mode, offset, length, fi);

			lm << std::make_pair(Format::SIZE, (long int)length)
			   << std::make_pair(Format::OFFSET, (long int)offset)
			   << std::make_pair(Format::MODE, (long int)mode);
        }
        break;
        case FSOperations::OP_BMAP:
        {
        	size_t blocksize = va_arg(ap, size_t);
        	uint64_t *idx = va_arg(ap, uint64_t*);
        	result = FSOperations::bmap(path, blocksize, idx);
        }
        break;
        case FSOperations::OP_IOCTL:
        {
        	int cmd = va_arg(ap, int);
        	void *arg = va_arg(ap, void*);
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	int flags = va_arg(ap, int);
        	void *data = va_arg(ap, void*);
        	result = FSOperations::ioctl(path, cmd, arg, fi, flags, data);
        }
        break;
        case FSOperations::OP_POLL:
        {
        	struct fuse_file_info *fi = (struct fuse_file_info*)va_arg(ap, void*);
        	struct fuse_pollhandle *ph = (struct fuse_pollhandle*)va_arg(ap, void*);
        	unsigned *reventsp = (unsigned*)va_arg(ap, unsigned*);
        	result = FSOperations::poll(path, fi, ph, reventsp);
        }
        break;
    }

    va_end(ap);

    Util::log(lm);

    return result;
}

int FSOperations::fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi){
	return ::fstat(fi->fh, stbuf)*errno;
}

int FSOperations::getattr(const char *path, struct stat *stbuf)
{
	return ::lstat(Util::getRelativePath(path).c_str(), stbuf)*errno;
}

int FSOperations::access(const char *path, int mask)
{
	return ::access(Util::getRelativePath(path).c_str(), mask)*errno;
}



int FSOperations::readlink(const char *path, char *buf, size_t size)
{
	std::fill(buf, buf+size, '\0');
	return ::readlink(Util::getRelativePath(path).c_str(), buf, size - 1)*errno;
}

int FSOperations::opendir(const char *path, struct fuse_file_info *fi)
{
	struct loggedfs_dirp *d = NULL;
	try{
		d = new loggedfs_dirp;
	}catch(...){}
	int res;
	if (d == NULL)
			return -ENOMEM;
	d->dp = ::opendir(Util::getRelativePath(path).c_str());

	if (d->dp == NULL) {
			res = -errno;
			delete d;
			return res;
	}
	d->offset = 0;
	d->entry = NULL;
	fi->fh = (unsigned long) d;
	return 0;
}
int FSOperations::readdir(const char *path, void *buf, fuse_fill_dir_t filler,
					   off_t offset, struct fuse_file_info *fi)
{
	struct loggedfs_dirp *d = (struct loggedfs_dirp *) (uintptr_t) fi->fh;
	(void) path;
	if (offset != d->offset) {
			::seekdir(d->dp, offset);
			d->entry = NULL;
			d->offset = offset;
	}
	while (1) {
			struct stat st;
			off_t nextoff;
			if (!d->entry) {
					d->entry = ::readdir(d->dp);
					if (!d->entry)
							break;
			}
			std::fill((char*)&st, ((char*)&st)+sizeof(st), '\0');
			st.st_ino = d->entry->d_ino;
			st.st_mode = d->entry->d_type << 12;
			nextoff = ::telldir(d->dp);
			if (filler(buf, d->entry->d_name, &st, nextoff))
					break;
			d->entry = NULL;
			d->offset = nextoff;
	}

	return 0;
}
int FSOperations::releasedir(const char *path, struct fuse_file_info *fi)
{
	struct loggedfs_dirp *d = (struct loggedfs_dirp *) (uintptr_t) fi->fh;
	(void) path;
	::closedir(d->dp);

	delete d;
	fi->fh = 0;
	return 0;
}

int FSOperations::mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;

	if (S_ISREG(mode)) {
		res = ::open(Util::getRelativePath(path).c_str(), O_CREAT | O_EXCL | O_WRONLY, mode);
		if (res >= 0)
			res = ::close(res);
	} else if (S_ISFIFO(mode))
	{
		res = ::mkfifo(Util::getRelativePath(path).c_str(), mode);
	}
	else
	{
		res = ::mknod(Util::getRelativePath(path).c_str(), mode, rdev);
	}

	if (res == -1)
		return -errno;

    return ::lchown(Util::getRelativePath(path).c_str(), fuse_get_context()->uid, fuse_get_context()->gid)*errno;
}

int FSOperations::mkdir(const char *path, mode_t mode)
{
	int res;
	res = ::mkdir(Util::getRelativePath(path).c_str(), mode);
	if(res == -1)
		return -errno;

	return ::lchown(Util::getRelativePath(path).c_str(), fuse_get_context()->uid, fuse_get_context()->gid)*errno;
}

int FSOperations::unlink(const char *path)
{
    return ::unlink(Util::getRelativePath(path).c_str())*errno;
}

int FSOperations::rmdir(const char *path)
{
    return ::rmdir(Util::getRelativePath(path).c_str())*errno;
}

int FSOperations::symlink(const char *from, const char *to)
{
	int res;
	res = ::symlink(Util::getRelativePath(from).c_str(), Util::getRelativePath(to).c_str());
	if(res == -1)
		return -errno;

	return ::lchown(Util::getRelativePath(to).c_str(), fuse_get_context()->uid, fuse_get_context()->gid)*errno;
}

int FSOperations::rename(const char *from, const char *to)
{
    return ::rename(Util::getRelativePath(from).c_str(), Util::getRelativePath(to).c_str())*errno;
}

int FSOperations::link(const char *from, const char *to)
{
	int res;
	res = ::link(Util::getRelativePath(from).c_str(), Util::getRelativePath(to).c_str());
	if(res == -1)
		return -errno;

 	return ::lchown(Util::getRelativePath(to).c_str(), fuse_get_context()->uid, fuse_get_context()->gid)*errno;
}

int FSOperations::chmod(const char *path, mode_t mode)
{
	return ::chmod(Util::getRelativePath(path).c_str(), mode)*errno;
}

int FSOperations::chown(const char *path, uid_t uid, gid_t gid)
{
	return ::lchown(Util::getRelativePath(path).c_str(), uid, gid) * errno;
}

int FSOperations::ftruncate(const char *path, off_t size, struct fuse_file_info *fi)
{
	return ::ftruncate(fi->fh, size)*errno;
}
int FSOperations::truncate(const char *path, off_t size)
{
    return ::truncate(Util::getRelativePath(path).c_str(), size)*errno;
}

int FSOperations::utimens(const char *path, const struct timespec ts[2])
{
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	return ::utimes(Util::getRelativePath(path).c_str(), tv)*errno;
}

int FSOperations::create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	fi->fh = ::creat(Util::getRelativePath(path).c_str(), mode);
	return (int)fi->fh==-1 ? -errno : 0;
}

int FSOperations::open(const char *path, struct fuse_file_info *fi)
{
    fi->fh = ::open(Util::getRelativePath(path).c_str(), fi->flags);
    return (int)fi->fh==-1 ? -errno : 0;
}

int FSOperations::read(const char *path, char *buf, size_t size, off_t offset,
						 struct fuse_file_info *fi)
{
	int res;
	res = ::pread(fi->fh, buf, size, offset);
	if(res == -1)
		res = -errno;

	return res;
}

int FSOperations::write(const char *path, const char *buf, size_t size,
						  off_t offset, struct fuse_file_info *fi)
{
	int res;
	res = ::pwrite(fi->fh, buf, size, offset);
	if(res == -1)
		res = -errno;

	return res;
}
int FSOperations::read_buf(const char *path, struct fuse_bufvec **bufp,
					size_t size, off_t offset, struct fuse_file_info *fi)
{
	try{
		struct fuse_bufvec *src;
		(void) path;
		src = new fuse_bufvec;
		if (src == NULL)
				return -ENOMEM;
		*src = FUSE_BUFVEC_INIT(size);
		src->buf[0].flags = (fuse_buf_flags) (FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
		src->buf[0].fd = fi->fh;
		src->buf[0].pos = offset;
		*bufp = src;
		return 0;
	}
	catch(...){}

	return -ENOMEM;
}

int FSOperations::write_buf(const char *path, struct fuse_bufvec *buf,
					 off_t offset, struct fuse_file_info *fi)
{
	struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));
	(void) path;
	dst.buf[0].flags = (fuse_buf_flags) (FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
	dst.buf[0].fd = fi->fh;
	dst.buf[0].pos = offset;
	return fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

int FSOperations::flush(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	/* This is called from every close on an open file, so call the
	   close on the underlying filesystem.  But since flush may be
	   called multiple times for an open file, this must not really
	   close the file.  This is important if used on a network
	   filesystem like NFS which flush the data/metadata on close() */
	return ::close(::dup(fi->fh))*errno;
}

int FSOperations::statfs(const char *path, struct statvfs *stbuf)
{
    return ::statvfs(Util::getRelativePath(path).c_str(), stbuf)*errno;
}

int FSOperations::release(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	::close(fi->fh);
	return 0;
}

int FSOperations::fsync(const char *path, int isdatasync, struct fuse_file_info *fi)
{
	(void) path;
	if (isdatasync)
  	    return ::fdatasync(fi->fh)*errno;
	else
		return ::fsync(fi->fh)*errno;
}
int FSOperations::fsyncdir(const char *path, int isdatasync, struct fuse_file_info *fi){
	(void) path;
	(void) isdatasync;
	(void) fi;

	return 0;
}

int FSOperations::setxattr(const char *path, const char *name, const char *value,
							 size_t size, int flags)
{
	return ::lsetxattr(Util::getRelativePath(path).c_str(), name, value, size, flags)*errno;
}

int FSOperations::getxattr(const char *path, const char *name, char *value,
							 size_t size)
{
	int res = ::lgetxattr(Util::getRelativePath(path).c_str(), name, value, size);
	if(res == -1)
		res = -errno;

	return res;
}

int FSOperations::listxattr(const char *path, char *list, size_t size)
{
	int res = ::llistxattr(Util::getRelativePath(path).c_str(), list, size);
	if(res == -1)
		res = -errno;

	return res;
}

int FSOperations::removexattr(const char *path, const char *name)
{
	return ::lremovexattr(Util::getRelativePath(path).c_str(), name)*errno;
}

/*
int FSOperations::lock(const char *path, struct fuse_file_info *fi, int cmd,
					struct flock *lock)
{
	(void) path;
	int res = ::ulockmgr_op(fi->fh, cmd, lock, &fi->lock_owner,
					   (size_t)sizeof(fi->lock_owner));
	return res;
}
*/
int FSOperations::flock(const char *path, struct fuse_file_info *fi, int op)
{
	(void) path;
	return ::flock(fi->fh, op)*errno;
}

int FSOperations::fallocate(const char *path, int mode,
						off_t offset, off_t length, struct fuse_file_info *fi)
{
	(void) path;
	if (mode)
		return -EOPNOTSUPP;
	else
		return -1*(errno = ::posix_fallocate(fi->fh, offset, length));
}

int FSOperations::bmap(const char *path, size_t blocksize, uint64_t *idx){
	(void) path;
	(void) blocksize;
	(void) idx;
	errno = EOPNOTSUPP;
	return -errno;
}
int FSOperations::ioctl(const char *path, int cmd, void *arg,
	      struct fuse_file_info *fi, unsigned int flags, void *data){
	(void) path;
	(void) cmd;
	(void) arg;
	(void) fi;
	(void) flags;
	(void) data;

	errno = EOPNOTSUPP;
	return -errno;
}
int FSOperations::poll(const char *path, struct fuse_file_info *fi,
	     struct fuse_pollhandle *ph, unsigned *reventsp){
	(void) path;
	(void) fi;
	(void) ph;
	(void) reventsp;

	errno = EOPNOTSUPP;
	return -errno;
}

fuse_operations& FSOperations::getFuseOperations(){
    static fuse_operations *loggedFS_operptr=NULL, loggedFS_oper;
    if(loggedFS_operptr==NULL){
    	loggedFS_operptr = & loggedFS_oper;

		// in case this code is compiled against a newer FUSE library and new
		// members have been added to fuse_operations, make sure they get set to
		// 0..
		std::fill((char*)&loggedFS_oper, ((char*)&loggedFS_oper) + sizeof(loggedFS_oper), '\0');
		loggedFS_oper.init		= FSOperations::init;

		loggedFS_oper.fgetattr	= (int (*) (const char *, struct stat *, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_FGETATTR>;
		loggedFS_oper.getattr	= (int (*)(const char*, struct stat*))&FSOperations::proxy<FSOperations::OP_GETATTR>;
		loggedFS_oper.access	= (int (*) (const char *, int))&FSOperations::proxy<FSOperations::OP_ACCESS>;
		loggedFS_oper.readlink	= (int (*) (const char *, char *, size_t))&FSOperations::proxy<FSOperations::OP_READLINK>;

		loggedFS_oper.opendir	= (int (*) (const char *, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_OPENDIR>;
		loggedFS_oper.readdir	= (int (*) (const char *, void *, fuse_fill_dir_t, off_t, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_READDIR>;
		loggedFS_oper.releasedir	= (int (*) (const char *, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_RELEASEDIR>;

		loggedFS_oper.mknod	= (int (*) (const char *, mode_t, dev_t))&FSOperations::proxy<FSOperations::OP_MKNOD>;
		loggedFS_oper.mkdir	= (int (*) (const char *, mode_t))&FSOperations::proxy<FSOperations::OP_MKDIR>;
		loggedFS_oper.symlink	= (int (*) (const char *, const char *))&FSOperations::proxy<FSOperations::OP_SYMLINK>;
		loggedFS_oper.unlink	= (int (*) (const char *))&FSOperations::proxy<FSOperations::OP_UNLINK>;
		loggedFS_oper.rmdir	= (int (*) (const char *))&FSOperations::proxy<FSOperations::OP_RMDIR>;

		loggedFS_oper.rename	= (int (*) (const char *, const char *))&FSOperations::proxy<FSOperations::OP_RENAME>;
		loggedFS_oper.link	= (int (*) (const char *, const char *))&FSOperations::proxy<FSOperations::OP_LINK>;
		loggedFS_oper.chmod	= (int (*) (const char *, mode_t))&FSOperations::proxy<FSOperations::OP_CHMOD>;
		loggedFS_oper.chown	= (int (*) (const char *, uid_t, gid_t))&FSOperations::proxy<FSOperations::OP_CHOWN>;

		loggedFS_oper.ftruncate	= (int (*) (const char *, off_t, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_FTRUNCATE>;
		loggedFS_oper.truncate	= (int (*) (const char *, off_t))&FSOperations::proxy<FSOperations::OP_TRUNCATE>;

		loggedFS_oper.utime	= (int (*) (const char *, struct utimbuf *))&FSOperations::proxy<FSOperations::OP_UTIME>;
		loggedFS_oper.utimens	= (int (*) (const char *, const struct timespec tv[2]))&FSOperations::proxy<FSOperations::OP_UTIMENS>;

		loggedFS_oper.read_buf	= (int (*) (const char *, struct fuse_bufvec **bufp, size_t size, off_t off, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_READ_BUF>;
		loggedFS_oper.write_buf	= (int (*) (const char *, struct fuse_bufvec *buf, off_t off, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_WRITE_BUF>;

		loggedFS_oper.flush	= (int (*) (const char *, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_FLUSH>;

		loggedFS_oper.create	= (int (*) (const char *, mode_t, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_CREATE>;
		loggedFS_oper.open	= (int (*) (const char *, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_OPEN>;
		loggedFS_oper.read	= (int (*) (const char *, char *, size_t, off_t, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_READ>;
		loggedFS_oper.write	= (int (*) (const char *, const char *, size_t, off_t, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_WRITE>;
		loggedFS_oper.statfs	= (int (*) (const char *, struct statvfs *))&FSOperations::proxy<FSOperations::OP_STATFS>;
		loggedFS_oper.release	= (int (*) (const char *, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_RELEASE>;

		loggedFS_oper.fsync	= (int (*) (const char *, int, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_FSYNC>;
		loggedFS_oper.fsyncdir	= (int (*) (const char *, int, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_FSYNCDIR>;

		loggedFS_oper.lock	= (int (*) (const char *, struct fuse_file_info *, int cmd, struct flock *))&FSOperations::proxy<FSOperations::OP_LOCK>;
		loggedFS_oper.flock	= (int (*) (const char *, struct fuse_file_info *, int op))&FSOperations::proxy<FSOperations::OP_FLOCK>;

		loggedFS_oper.fallocate	= (int (*) (const char *, int, off_t, off_t, struct fuse_file_info *))&FSOperations::proxy<FSOperations::OP_FALLOCATE>;

		loggedFS_oper.setxattr	= (int (*) (const char *, const char *, const char *, size_t, int))&FSOperations::proxy<FSOperations::OP_SETXATTR>;
		loggedFS_oper.getxattr	= (int (*) (const char *, const char *, char *, size_t))&FSOperations::proxy<FSOperations::OP_GETXATTR>;
		loggedFS_oper.listxattr	= (int (*) (const char *, char *, size_t))&FSOperations::proxy<FSOperations::OP_LISTXATTR>;
		loggedFS_oper.removexattr	= (int (*) (const char *, const char *))&FSOperations::proxy<FSOperations::OP_REMOVEXATTR>;

		loggedFS_oper.bmap	= (int (*) (const char *, size_t blocksize, uint64_t *idx))&FSOperations::proxy<FSOperations::OP_BMAP>;
		loggedFS_oper.ioctl = (int (*) (const char *, int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data))&FSOperations::proxy<FSOperations::OP_IOCTL>;
		loggedFS_oper.poll = (int (*) (const char *, struct fuse_file_info *, struct fuse_pollhandle *ph, unsigned *reventsp))&FSOperations::proxy<FSOperations::OP_POLL>;

    }

    return loggedFS_oper;
}
