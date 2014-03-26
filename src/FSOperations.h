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

    	OP_FGETATTR,
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
		OP_FTRUNCATE,
		OP_TRUNCATE,
		OP_UTIME, // deprecated
		OP_UTIMENS,
		OP_READ_BUF,
		OP_WRITE_BUF,
		OP_FLUSH,
		OP_CREATE,
		OP_OPEN,
		OP_READ,
		OP_WRITE,
		OP_STATFS,
		OP_RELEASE,
		OP_FSYNC,
		OP_FSYNCDIR,
		OP_FALLOCATE,
		OP_SETXATTR,
		OP_GETXATTR,
		OP_LISTXATTR,
		OP_REMOVEXATTR,
		OP_LOCK,
		OP_FLOCK,

		OP_BMAP,
		OP_IOCTL,
		OP_POLL
    };

    template <int N> static int proxy(const char *path, ...);

	static void* init(struct fuse_conn_info* info);

	static int fgetattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
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
	static int ftruncate(const char *path, off_t size, struct fuse_file_info *fi);
	static int truncate(const char *path, off_t size);

	static int utimens(const char *path, const struct timespec ts[2]);

	static int create(const char *path, mode_t mode, struct fuse_file_info *fi);
	static int open(const char *path, struct fuse_file_info *fi);
	static int read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
	static int write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
	static int read_buf(const char *path, struct fuse_bufvec **bufp, size_t size, off_t offset, struct fuse_file_info *fi);
	static int write_buf(const char *path, struct fuse_bufvec *buf, off_t offset, struct fuse_file_info *fi);
	static int flush(const char *path, struct fuse_file_info *fi);
	static int statfs(const char *path, struct statvfs *stbuf);
	static int release(const char *path, struct fuse_file_info *fi);

	static int fsync(const char *path, int isdatasync, struct fuse_file_info *fi);
	static int fsyncdir(const char *path, int isdatasync, struct fuse_file_info *fi);

	static int setxattr(const char *path, const char *name, const char *value, size_t size, int flags);
	static int getxattr(const char *path, const char *name, char *value, size_t size);
	static int listxattr(const char *path, char *list, size_t size);
	static int removexattr(const char *path, const char *name);


	//static int lock(const char *path, struct fuse_file_info *fi, int cmd, struct flock *lock);

	static int flock(const char *path, struct fuse_file_info *fi, int op);
	static int fallocate(const char *path, int mode,
							off_t offset, off_t length, struct fuse_file_info *fi);

	static int bmap(const char *, size_t blocksize, uint64_t *idx);
	static int ioctl(const char *, int cmd, void *arg,
		      struct fuse_file_info *, unsigned int flags, void *data);
	static int poll(const char *, struct fuse_file_info *,
		     struct fuse_pollhandle *ph, unsigned *reventsp);

	static fuse_operations& getFuseOperations();

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
