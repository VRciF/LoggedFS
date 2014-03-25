
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <utime.h>
#include <sys/time.h>
#include <errno.h>

#include <sys/file.h>

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
		FSOperations::actions[OP_TRUNCATE]="truncate";
		FSOperations::actions[OP_UTIME]="utime";
		FSOperations::actions[OP_UTIMENS]="utimens";
		FSOperations::actions[OP_READ_BUF]="read_buf";
		FSOperations::actions[OP_WRITE_BUF]="write_buf";
		FSOperations::actions[OP_FLUSH]="flush";
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
		//FSOperations::actions[OP_LOCK]="lock";
		FSOperations::actions[OP_FLOCK]="flock";
	}

	 return NULL;
}

int FSOperations::getattr(const char *path, struct stat *stbuf)
{

	errno = 0;

	int res=0;
	res = ::lstat(Util::getRelativePath(path).c_str(), stbuf);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_GETATTR)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

	if(res == -1)
		return -errno;

	return 0;
}

int FSOperations::access(const char *path, int mask)
{
	errno = 0;

	int res;

	res = ::access(Util::getRelativePath(path).c_str(), mask);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_ACCESS)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

	if (res == -1)
		return -errno;

	return 0;
}



int FSOperations::readlink(const char *path, char *buf, size_t size)
{
	errno = 0;

	int res;

	res = ::readlink(Util::getRelativePath(path).c_str(), buf, size - 1);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_READLINK)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

	if(res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}

int FSOperations::opendir(const char *path, struct fuse_file_info *fi)
{
	errno = 0;

	struct loggedfs_dirp *d = NULL;
	try{
		d = new loggedfs_dirp;
	}catch(...){}
	int res;
	if (d == NULL)
			return -ENOMEM;
	d->dp = ::opendir(Util::getRelativePath(path).c_str());

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_OPENDIR)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

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
	errno = 0;

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

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_READDIR)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

	return 0;
}
int FSOperations::releasedir(const char *path, struct fuse_file_info *fi)
{
	errno = 0;

	struct loggedfs_dirp *d = (struct loggedfs_dirp *) (uintptr_t) fi->fh;
	(void) path;
	::closedir(d->dp);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_RELEASEDIR)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

	delete d;
	fi->fh = 0;
	return 0;
}

int FSOperations::mknod(const char *path, mode_t mode, dev_t rdev)
{
	errno = 0;

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
	else
		{
		::lchown(Util::getRelativePath(path).c_str(), fuse_get_context()->uid, fuse_get_context()->gid);
		}

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_MKNOD)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::MODE, (long int)mode)
			   << std::make_pair(Format::RDEV, (long int)rdev)
		);
	}

	return 0;
}

int FSOperations::mkdir(const char *path, mode_t mode)
{
	errno = 0;

	int res;

	res = ::mkdir(Util::getRelativePath(path).c_str(), mode);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_MKDIR)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::MODE, (long int)mode)
		);
	}

	if(res == -1)
		return -errno;
	else
		lchown(Util::getRelativePath(path).c_str(), fuse_get_context()->uid, fuse_get_context()->gid);
	return 0;
}

int FSOperations::unlink(const char *path)
{
	errno = 0;

	int res;

	res = ::unlink(Util::getRelativePath(path).c_str());

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_UNLINK)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

	if(res == -1)
		return -errno;

	return 0;
}

int FSOperations::rmdir(const char *path)
{
	errno = 0;

	int res;

	res = ::rmdir(Util::getRelativePath(path).c_str());

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_RMDIR)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

	if(res == -1)
		return -errno;

	return 0;
}

int FSOperations::symlink(const char *from, const char *to)
{
	errno = 0;

	int res;

	res = ::symlink(Util::getRelativePath(from).c_str(), Util::getRelativePath(to).c_str());

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_SYMLINK)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, from)
			   << std::make_pair(Format::RELFROM, from)
			   << std::make_pair(Format::RELTO, to)
			   << std::make_pair(Format::ABSFROM, Util::getAbsolutePath(from))
			   << std::make_pair(Format::ABSTO, Util::getAbsolutePath(to))
		);
	}

	if(res == -1)
		return -errno;
	else
		::lchown(Util::getRelativePath(to).c_str(), fuse_get_context()->uid, fuse_get_context()->gid);

	return 0;
}

int FSOperations::rename(const char *from, const char *to)
{
	errno = 0;

	int res;

	res = ::rename(Util::getRelativePath(from).c_str(), Util::getRelativePath(to).c_str());

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_RENAME)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, from)
			   << std::make_pair(Format::RELFROM, from)
			   << std::make_pair(Format::RELTO, to)
			   << std::make_pair(Format::ABSFROM, Util::getAbsolutePath(from))
			   << std::make_pair(Format::ABSTO, Util::getAbsolutePath(to))
		);
	}

	if(res == -1)
		return -errno;

	return 0;
}

int FSOperations::link(const char *from, const char *to)
{
	errno = 0;

	int res;

	res = ::link(Util::getRelativePath(from).c_str(), Util::getRelativePath(to).c_str());

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_LINK)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, from)
			   << std::make_pair(Format::RELFROM, from)
			   << std::make_pair(Format::RELTO, to)
			   << std::make_pair(Format::ABSFROM, Util::getAbsolutePath(from))
			   << std::make_pair(Format::ABSTO, Util::getAbsolutePath(to))
		);
	}

	if(res == -1)
		return -errno;
	else
		::lchown(Util::getRelativePath(to).c_str(), fuse_get_context()->uid, fuse_get_context()->gid);

	return 0;
}

int FSOperations::chmod(const char *path, mode_t mode)
{
	errno = 0;

	int res;

	res = ::chmod(Util::getRelativePath(path).c_str(), mode);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_CHMOD)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::MODE, (long int)mode)
		);
	}

	if(res == -1)
		return -errno;

	return 0;
}

int FSOperations::chown(const char *path, uid_t uid, gid_t gid)
{
	errno = 0;

	int res;

	res = ::lchown(Util::getRelativePath(path).c_str(), uid, gid);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_CHOWN)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::UID, (long int)uid)
			   << std::make_pair(Format::UID, (long int)gid)
		);
	}

	if(res == -1)
		return -errno;

	return 0;
}

int FSOperations::truncate(const char *path, off_t size)
{
	errno = 0;

	int res;

	res = ::truncate(Util::getRelativePath(path).c_str(), size);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_TRUNCATE)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::SIZE, (long int)size)
		);
	}

	if(res == -1)
		return -errno;

	return 0;
}

#if (FUSE_USE_VERSION==25)
int FSOperations::utime(const char *path, struct utimbuf *buf)
{
	errno = 0;

	int res;

	res = ::utime(Util::getRelativePath(path).c_str(), buf);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_UTIME)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::MODSECONDS, (long int)buf->modtime)
		       << std::make_pair(Format::MODMICROSECONDS, (long int)0)
		       << std::make_pair(Format::ACSECONDS, (long int)buf->actime)
		       << std::make_pair(Format::ACMICROSECONDS, (long int)0)
		);
	}

	if(res == -1)
		return -errno;

	return 0;
}

#else


int FSOperations::utimens(const char *path, const struct timespec ts[2])
{
	errno = 0;

	int res;
	struct timeval tv[2];

	tv[0].tv_sec = ts[0].tv_sec;
	tv[0].tv_usec = ts[0].tv_nsec / 1000;
	tv[1].tv_sec = ts[1].tv_sec;
	tv[1].tv_usec = ts[1].tv_nsec / 1000;

	res = ::utimes(Util::getRelativePath(path).c_str(), tv);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_UTIMENS)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::MODSECONDS, (long int)ts[1].tv_sec)
		       << std::make_pair(Format::MODMICROSECONDS, (long int)ts[1].tv_nsec)
		       << std::make_pair(Format::ACSECONDS, (long int)ts[0].tv_sec)
		       << std::make_pair(Format::ACMICROSECONDS, (long int)ts[0].tv_nsec)
		);
	}

	if (res == -1)
		return -errno;

	return 0;
}

#endif

int FSOperations::open(const char *path, struct fuse_file_info *fi)
{
	errno = 0;

	int res;

	res = ::open(Util::getRelativePath(path).c_str(), fi->flags);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_OPEN)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::FLAGS, (long int)fi->flags)
		);
	}

	if(res == -1)
		return -errno;

	fi->fh = res;
	return 0;
}

int FSOperations::read(const char *path, char *buf, size_t size, off_t offset,
						 struct fuse_file_info *fi)
{
	errno = 0;

	int res;

	res = ::pread(fi->fh, buf, size, offset);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_READ)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::SIZE, (long int)size)
			   << std::make_pair(Format::OFFSET, (long int)offset)
		);
	}

	if(res == -1)
		res = -errno;

	return res;
}

int FSOperations::write(const char *path, const char *buf, size_t size,
						  off_t offset, struct fuse_file_info *fi)
{
	errno = 0;

	int res;

	res = ::pwrite(fi->fh, buf, size, offset);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_WRITE)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::SIZE, (long int)size)
			   << std::make_pair(Format::OFFSET, (long int)offset)
		);
	}

	if(res == -1)
		res = -errno;

	return res;
}
int FSOperations::read_buf(const char *path, struct fuse_bufvec **bufp,
					size_t size, off_t offset, struct fuse_file_info *fi)
{
	errno = 0;

	try{
		if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_READ_BUF)){
			LogMap lm;
			Util::log(
				lm << std::make_pair(Format::ACTION, __FUNCTION__)
				   << std::make_pair(Format::RELPATH, path)
				   << std::make_pair(Format::SIZE, (long int)size)
				   << std::make_pair(Format::OFFSET, (long int)offset)
			);
		}

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
	errno = 0;

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_WRITE_BUF)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::SIZE, (long int)fuse_buf_size(buf))
			   << std::make_pair(Format::OFFSET, (long int)offset)
		);
	}

	struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));
	(void) path;
	dst.buf[0].flags = (fuse_buf_flags) (FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
	dst.buf[0].fd = fi->fh;
	dst.buf[0].pos = offset;
	return fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

int FSOperations::flush(const char *path, struct fuse_file_info *fi)
{
	errno = 0;

	int res;
	(void) path;
	/* This is called from every close on an open file, so call the
	   close on the underlying filesystem.  But since flush may be
	   called multiple times for an open file, this must not really
	   close the file.  This is important if used on a network
	   filesystem like NFS which flush the data/metadata on close() */
	res = ::close(::dup(fi->fh));

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_FLUSH)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

	if (res == -1)
			return -errno;
	return 0;
}

int FSOperations::statfs(const char *path, struct statvfs *stbuf)
{
	errno = 0;

	int res;

	res = ::statvfs(Util::getRelativePath(path).c_str(), stbuf);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_STATFS)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

	if(res == -1)
		return -errno;

	return 0;
}

int FSOperations::release(const char *path, struct fuse_file_info *fi)
{
	errno = 0;

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_RELEASE)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

	(void) path;
	::close(fi->fh);
	return 0;
}

int FSOperations::fsync(const char *path, int isdatasync,
						  struct fuse_file_info *fi)
{
	errno = 0;

	int res;
	(void) path;
	#ifndef HAVE_FDATASYNC
			(void) isdatasync;
	#else
			if (isdatasync)
					res = ::fdatasync(fi->fh);
			else
	#endif
					res = ::fsync(fi->fh);

    if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_FSYNC)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
    }

	if (res == -1)
			return -errno;
	return 0;
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
int FSOperations::setxattr(const char *path, const char *name, const char *value,
							 size_t size, int flags)
{
	errno = 0;

	int res = ::lsetxattr(Util::getRelativePath(path).c_str(), name, value, size, flags);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_SETXATTR)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::XATTRNAME, name)
			   << std::make_pair(Format::XATTRVALUE, std::string(value,size))
			   << std::make_pair(Format::SIZE, (long int)size)
			   << std::make_pair(Format::FLAGS, (long int)flags)
		);
	}

	if(res == -1)
		return -errno;
	return 0;
}

int FSOperations::getxattr(const char *path, const char *name, char *value,
							 size_t size)
{
	errno = 0;

	int res = ::lgetxattr(Util::getRelativePath(path).c_str(), name, value, size);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_GETXATTR)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::XATTRNAME, name)
			   << std::make_pair(Format::XATTRVALUE, std::string(value,size))
			   << std::make_pair(Format::SIZE, (long int)size)
		);
	}

	if(res == -1)
		return -errno;
	return res;
}

int FSOperations::listxattr(const char *path, char *list, size_t size)
{
	errno = 0;

	int res = ::llistxattr(Util::getRelativePath(path).c_str(), list, size);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_LISTXATTR)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::XATTRLIST, std::string(list, size))
			   << std::make_pair(Format::SIZE, (long int)size)
		);
	}

	if(res == -1)
		return -errno;
	return res;
}

int FSOperations::removexattr(const char *path, const char *name)
{
	errno = 0;

	int res = ::lremovexattr(Util::getRelativePath(path).c_str(), name);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_REMOVEXATTR)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

	if(res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */


/*
int FSOperations::lock(const char *path, struct fuse_file_info *fi, int cmd,
					struct flock *lock)
{
    errno = 0;

	(void) path;
	int res = ::ulockmgr_op(fi->fh, cmd, lock, &fi->lock_owner,
					   (size_t)sizeof(fi->lock_owner));
	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_LOCK)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
		);
	}

	return res;
}
*/
int FSOperations::flock(const char *path, struct fuse_file_info *fi, int op)
{
	errno = 0;

	int res;
	(void) path;
	res = ::flock(fi->fh, op);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_FLOCK)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::FLAGS, (long int)op)
		);
	}

	if (res == -1)
			return -errno;
	return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
int FSOperations::fallocate(const char *path, int mode,
						off_t offset, off_t length, struct fuse_file_info *fi)
{
	errno = 0;

	(void) path;
	if (mode)
		errno = EOPNOTSUPP;
	else
		errno = ::posix_fallocate(fi->fh, offset, length);

	if(Globals::instance()->config.fuzzyShouldLog(FSOperations::OP_FALLOCATE)){
		LogMap lm;
		Util::log(
			lm << std::make_pair(Format::ACTION, __FUNCTION__)
			   << std::make_pair(Format::RELPATH, path)
			   << std::make_pair(Format::SIZE, (long int)length)
			   << std::make_pair(Format::OFFSET, (long int)offset)
			   << std::make_pair(Format::MODE, (long int)mode)
		);
	}

	return -errno;
}
#endif
