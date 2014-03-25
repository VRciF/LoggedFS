#ifndef LOGGEDFS_FSOPERATIONS_H
#define LOGGEDFS_FSOPERATIONS_H

#include <sys/types.h>
#include <dirent.h>

#include <fuse.h>

class FSOperations{
protected:
	struct loggedfs_dirp {
	        DIR *dp;
	        struct dirent *entry;
	        off_t offset;
	};

public:

    static std::map<int, std::string> actions;
    enum Operations{
    	OP_NONE=0,

	    OP_GETATTR,
	    OP_ACCESS,
	    OP_READLINK,
	    OP_OPENDIR,
		OP_READDIR,
		OP_RELEASEDIR,
		OP_MKNOD,
		OP_MKDIR,
		OP_SYMLINK,
		OP_UNLINK,
		OP_RMDIR,
		OP_RENAME,
		OP_LINK,
		OP_CHMOD,
		OP_CHOWN,
		OP_TRUNCATE,
		OP_UTIME,
		OP_UTIMENS,
		OP_READ_BUF,
		OP_WRITE_BUF,
		OP_FLUSH,
		OP_OPEN,
		OP_READ,
		OP_WRITE,
		OP_STATFS,
		OP_RELEASE,
		OP_FSYNC,
		OP_FALLOCATE,
		OP_SETXATTR,
		OP_GETXATTR,
		OP_LISTXATTR,
		OP_REMOVEXATTR,
		//OP_LOCK,
		OP_FLOCK
    };

	static void* init(struct fuse_conn_info* info);

	static int getattr(const char *path, struct stat *stbuf);
	static int access(const char *path, int mask);
	static int readlink(const char *path, char *buf, size_t size);
	static int opendir(const char *path, struct fuse_file_info *fi);
	static int readdir(const char *path, void *buf, fuse_fill_dir_t filler, off_t offset, struct fuse_file_info *fi);
	static int releasedir(const char *path, struct fuse_file_info *fi);
	static int mknod(const char *path, mode_t mode, dev_t rdev);
	static int mkdir(const char *path, mode_t mode);
	static int unlink(const char *path);
	static int rmdir(const char *path);
	static int symlink(const char *from, const char *to);
	static int rename(const char *from, const char *to);
	static int link(const char *from, const char *to);
	static int chmod(const char *path, mode_t mode);
	static int chown(const char *path, uid_t uid, gid_t gid);
	static int truncate(const char *path, off_t size);
	#if (FUSE_USE_VERSION==25)
	static int utime(const char *path, struct utimbuf *buf);
	#else
	static int utimens(const char *path, const struct timespec ts[2]);
	#endif

	static int open(const char *path, struct fuse_file_info *fi);
	static int read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
	static int write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
	static int read_buf(const char *path, struct fuse_bufvec **bufp, size_t size, off_t offset, struct fuse_file_info *fi);
	static int write_buf(const char *path, struct fuse_bufvec *buf, off_t offset, struct fuse_file_info *fi);
	static int flush(const char *path, struct fuse_file_info *fi);
	static int statfs(const char *path, struct statvfs *stbuf);
	static int release(const char *path, struct fuse_file_info *fi);
	static int fsync(const char *path, int isdatasync, struct fuse_file_info *fi);

	#ifdef HAVE_SETXATTR
	/* xattr operations are optional and can safely be left unimplemented */
	static int setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
	static int getxattr(const char *path, const char *name, char *value, size_t size);
	static int listxattr(const char *path, char *list, size_t size);
	static int removexattr(const char *path, const char *name);
	#endif /* HAVE_SETXATTR */


	//static int lock(const char *path, struct fuse_file_info *fi, int cmd, struct flock *lock);

	static int flock(const char *path, struct fuse_file_info *fi, int op);
	#ifdef HAVE_POSIX_FALLOCATE
	static int fallocate(const char *path, int mode,
							off_t offset, off_t length, struct fuse_file_info *fi);
	#endif

}; // END OF CLASS FSOperations

class LogMap{
	public:
		std::map<int, std::string> values;
		LogMap& add(int key, std::string value){ values[key] = value; return *this; }
};
LogMap& operator<<(LogMap &what, std::pair<Format::Specifier,char *> kv);
LogMap& operator<<(LogMap &what, std::pair<Format::Specifier,const char *> kv);
LogMap& operator<<(LogMap &what, std::pair<Format::Specifier,std::string> kv);
LogMap& operator<<(LogMap &what, std::pair<Format::Specifier,long int> kv);

#endif
