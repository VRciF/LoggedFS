/*****************************************************************************
 * Author:   Remi Flament <rflament at laposte dot net>
 *****************************************************************************
 * Copyright (c) 2005 - 2008, Remi Flament
 *
 * This library is free software; you can distribute it and/or modify it under
 * the terms of the GNU General Public License (GPL), as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This library is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GPL in the file COPYING for more
 * details.
 *
 */

/* Almost the whole code has been recopied from encfs source code and from fusexmp.c
*/

/*
 * MISSING:
 *  *) call the new loggedfs_log function
 */

#ifdef linux
/* For pread()/pwrite() */
#define _X_SOURCE 500
#endif

#include <fuse.h>
//#include <ulockmgr.h> /*gcc -Wall fusexmp_fh.c pkg-config fuse --cflags --libs -lulockmgr -o fusexmp_fh*/
#include <unistd.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <sys/statfs.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <rlog/rlog.h>
#include <rlog/Error.h>
#include <rlog/RLogChannel.h>
#include <rlog/SyslogNode.h>
#include <rlog/StdioNode.h>
#include <stdarg.h>
#include <getopt.h>
#include <sys/time.h>
#include <pwd.h>
#include <grp.h>

#include <sys/file.h>
#include <signal.h>

#include <iostream>
#include <sstream>
#include <map>

#include "Config.h"

#define PUSHARG(ARG) \
rAssert(out->fuseArgc < MaxFuseArgs); \
out->fuseArgv[out->fuseArgc++] = ARG

using namespace std;
using namespace rlog;


static RLogChannel *Info = DEF_CHANNEL("info", Log_Info);
static Config config;
static int savefd;
static std::string logfilename;
static int fileLog=-1;
static StdioNode* fileLogNode=NULL;

const int MaxFuseArgs = 32;
struct LoggedFS_Args
{
    std::string mountPoint; // where the users read files
    std::string configFilename;
    bool isDaemon; // true == spawn in background, log to syslog
    const char *fuseArgv[MaxFuseArgs];
    int fuseArgc;
};


static LoggedFS_Args *loggedfsArgs = new LoggedFS_Args;


static bool isAbsolutePath( const std::string fileName )
{
    if(fileName.length()>0 && fileName[0] != '\0' && fileName[0] == '/')
        return true;
    else
        return false;
}


static std::string getAbsolutePath(const char *path)
{
	std::string realPath;
	if(path==NULL) return realPath;

	realPath += loggedfsArgs->mountPoint;
	realPath += path;

	return realPath;
}

static std::string getRelativePath(const char* path)
{
	std::string rPath;
	if(path==NULL) return rPath;

	rPath += ".";
	rPath += path;

	return rPath;
}

/*
 * Returns the name of the process which accessed the file system.
 */
static std::string getcallername()
{
    char filename[100];
    snprintf(filename, sizeof(filename),"/proc/%d/cmdline",fuse_get_context()->pid);
    FILE * proc=fopen(filename,"rt");
    char cmdline[256]="";
    if(proc){
        fread(cmdline,sizeof(cmdline),1,proc);
        fclose(proc);
    }
    return std::string(cmdline);
}

static void loggedfs_formatlog(const std::string format, const std::map<int, std::string> *values, std::string *newbuf){
	if(newbuf==NULL) return;
	try{
		// initialize default values used only once
		// used in case if not set in 'values' parameter but requested in format
		static std::map<int, std::string> parentvalues;
		if(parentvalues.size()<=0){
			parentvalues[Config::FORMAT_ACTION] = "";
			parentvalues[Config::FORMAT_ERRNO] = "";
			parentvalues[Config::FORMAT_CMDNAME] = "";
		}

		struct fuse_context *ctx = fuse_get_context();

		// inherit parentvalues from workingset
		std::map<int, std::string> workingset;
		if(values!=NULL)
			workingset = *values;

		std::map<int, std::string>* formatstrings = Config::formatstrings();
		std::map<int, std::string>::iterator fsiter;
		std::map<int, off_t> formatpositions;
		std::map<int, off_t>::iterator fpiter;
		*newbuf = format;
		std::stringstream ss;

		for(fsiter = formatstrings->begin(); fsiter != formatstrings->end(); ++fsiter){
			std::size_t pos = format.find(fsiter->second);
			if(pos!=std::string::npos){
				formatpositions[fsiter->first] = pos;

				if(ctx){
					ss.ignore();
					switch(fsiter->first){
						case Config::FORMAT_REQPID:
							ss << ctx->pid;
							workingset[Config::FORMAT_REQPID] = ss.str();
							break;
						case Config::FORMAT_REQUID:
							ss << ctx->uid;
							workingset[Config::FORMAT_REQUID] = ss.str();
							break;
						case Config::FORMAT_REQGID:
							ss << ctx->gid;
							workingset[Config::FORMAT_REQGID] = ss.str();
							break;
						case Config::FORMAT_REQUMASK:
							ss << ctx->umask;
							workingset[Config::FORMAT_REQUMASK] = ss.str();
							break;
						case Config::FORMAT_CMDNAME:
							workingset[Config::FORMAT_CMDNAME] = getcallername();
							break;
					}
				}
			}
		}
		for(fpiter = formatpositions.begin(); fpiter != formatpositions.end(); ++fpiter){
			newbuf->replace(fpiter->second, formatstrings[fpiter->first].size(), workingset[fpiter->first]);
		}
	}catch(std::exception e){
		rError( "format log message failed: %s", e.what());
	}
}

static void loggedfs_log(const std::map<int, std::string> *values)
{
	if(values==NULL) return;

	std::map<int, std::string>::const_iterator iter;

	iter = values->find(Config::FORMAT_ERRNO);
	if(iter==values->end()) return;
	const char *retname = iter->second.compare("0")==0?"SUCCESS":"FAILURE";

	iter = values->find(Config::FORMAT_ACTION);
	if(iter==values->end()) return;
	const char *action = iter->second.c_str();

	iter = values->find(Config::FORMAT_RELPATH);
	if(iter==values->end()) return;
	const char *path = iter->second.c_str();

	std::string formattemplate;
	if (config.shouldLog(path,fuse_get_context()->uid,action,retname, formattemplate)){
		std::string message;
		loggedfs_formatlog(formattemplate, values, &message);
		rLog(Info, message.c_str());
    }
}
/*
static void loggedfs_log(const char* path,const char* action,const int returncode,const char *format,...)
{
	char *format;
    char *retname;
    if (returncode >= 0)
        retname = "SUCCESS";
    else
        retname = "FAILURE";

    char *formattemplate;
    if (config.shouldLog(path,fuse_get_context()->uid,action,retname))
    {
        va_list args;
        char buf[1024];
        va_start(args,format);
        memset(buf,0,1024);
        vsprintf(buf,format,args);
        strcat(buf," {%s} [ pid = %d %s uid = %d ]");
        if (returncode >= 0)
		    rLog(Info, buf,retname, fuse_get_context()->pid,config.isPrintProcessNameEnabled()?getcallername():"", fuse_get_context()->uid);
	    else
		    rError( buf,retname, fuse_get_context()->pid,config.isPrintProcessNameEnabled()?getcallername():"", fuse_get_context()->uid);
        va_end(args);
    }
}
*/
static void* loggedFS_init(struct fuse_conn_info* info)
{
     fchdir(savefd);
     close(savefd);
     return NULL;
}


static int loggedFS_getattr(const char *path, struct stat *stbuf)
{
    int res;

    res = lstat(getRelativePath(path).c_str(), stbuf);
    //loggedfs_log(aPath,"getattr",res,"getattr %s",aPath);
    loggedfs_log(NULL);
    getAbsolutePath(path);
    if(res == -1)
        return -errno;

    return 0;
}


static int loggedFS_access(const char *path, int mask)
{
    int res;

    res = access(getRelativePath(path).c_str(), mask);
    //loggedfs_log(aPath,"access",res,"access %s",aPath);
    if (res == -1)
        return -errno;

    return 0;
}



static int loggedFS_readlink(const char *path, char *buf, size_t size)
{
    int res;

    res = readlink(getRelativePath(path).c_str(), buf, size - 1);
    //loggedfs_log(aPath,"readlink",res,"readlink %s",aPath);
    if(res == -1)
        return -errno;

    buf[res] = '\0';
    return 0;
}



struct loggedfs_dirp {
        DIR *dp;
        struct dirent *entry;
        off_t offset;
};
static int loggedFS_opendir(const char *path, struct fuse_file_info *fi)
{
	struct loggedfs_dirp *d = NULL;
	try{
		d = new loggedfs_dirp;
	}catch(...){}
	int res;
	if (d == NULL)
			return -ENOMEM;
	d->dp = opendir(getRelativePath(path).c_str());
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
static int loggedFS_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                       off_t offset, struct fuse_file_info *fi)
{
        struct loggedfs_dirp *d = (struct loggedfs_dirp *) (uintptr_t) fi->fh;
        (void) path;
        if (offset != d->offset) {
                seekdir(d->dp, offset);
                d->entry = NULL;
                d->offset = offset;
        }
        while (1) {
                struct stat st;
                off_t nextoff;
                if (!d->entry) {
                        d->entry = readdir(d->dp);
                        if (!d->entry)
                                break;
                }
                std::fill((char*)&st, ((char*)&st)+sizeof(st), '\0');
                st.st_ino = d->entry->d_ino;
                st.st_mode = d->entry->d_type << 12;
                nextoff = telldir(d->dp);
                if (filler(buf, d->entry->d_name, &st, nextoff))
                        break;
                d->entry = NULL;
                d->offset = nextoff;
        }

        //loggedfs_log(aPath,"readdir",0,"readdir %s",aPath);
        return 0;
}
static int loggedFS_releasedir(const char *path, struct fuse_file_info *fi)
{
        struct loggedfs_dirp *d = (struct loggedfs_dirp *) (uintptr_t) fi->fh;
        (void) path;
        closedir(d->dp);
        delete d;
        fi->fh = 0;
        return 0;
}









static int loggedFS_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int res;

    if (S_ISREG(mode)) {
        res = open(getRelativePath(path).c_str(), O_CREAT | O_EXCL | O_WRONLY, mode);
	//loggedfs_log(aPath,"mknod",res,"mknod %s %o S_IFREG (normal file creation)",aPath, mode);
        if (res >= 0)
            res = close(res);
    } else if (S_ISFIFO(mode))
	{
        res = mkfifo(getRelativePath(path).c_str(), mode);
	//loggedfs_log(aPath,"mkfifo",res,"mkfifo %s %o S_IFFIFO (fifo creation)",aPath, mode);
	}
    else
	{
        res = mknod(getRelativePath(path).c_str(), mode, rdev);
	if (S_ISCHR(mode))
		{
	        //loggedfs_log(aPath,"mknod",res,"mknod %s %o (character device creation)",aPath, mode);
		}
	/*else if (S_IFBLK(mode))
		{
		//loggedfs_log(aPath,"mknod",res,"mknod %s %o (block device creation)",aPath, mode);
		}*/
	else ;//loggedfs_log(aPath,"mknod",res,"mknod %s %o",aPath, mode);
	}

	if (res == -1)
	        return -errno;
	else
		{
		lchown(getRelativePath(path).c_str(), fuse_get_context()->uid, fuse_get_context()->gid);
		}

    return 0;
}

static int loggedFS_mkdir(const char *path, mode_t mode)
{
    int res;

    res = mkdir(getRelativePath(path).c_str(), mode);
    //loggedfs_log(aPath, "mkdir",res,"mkdir %s %o",aPath, mode);
    if(res == -1)
        return -errno;
    else
        lchown(getRelativePath(path).c_str(), fuse_get_context()->uid, fuse_get_context()->gid);
    return 0;
}

static int loggedFS_unlink(const char *path)
{
    int res;

    res = unlink(getRelativePath(path).c_str());
    //loggedfs_log(aPath,"unlink",res,"unlink %s",aPath);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_rmdir(const char *path)
{
    int res;

    res = rmdir(getRelativePath(path).c_str());
    //loggedfs_log(aPath,"rmdir",res,"rmdir %s",aPath);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_symlink(const char *from, const char *to)
{
    int res;

    res = symlink(getRelativePath(from).c_str(), getRelativePath(to).c_str());
    
    //loggedfs_log( aTo,"symlink",res,"symlink from %s to %s",aTo,from);
    if(res == -1)
        return -errno;
    else
        lchown(getRelativePath(to).c_str(), fuse_get_context()->uid, fuse_get_context()->gid);

    return 0;
}

static int loggedFS_rename(const char *from, const char *to)
{
    int res;

    res = rename(getRelativePath(from).c_str(), getRelativePath(to).c_str());
    //loggedfs_log( aFrom,"rename",res,"rename %s to %s",aFrom,aTo);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_link(const char *from, const char *to)
{
    int res;

    res = link(getRelativePath(from).c_str(), getRelativePath(to).c_str());
    //loggedfs_log( aTo,"link",res,"hard link from %s to %s",aTo,aFrom);
    if(res == -1)
        return -errno;
    else
        lchown(getRelativePath(to).c_str(), fuse_get_context()->uid, fuse_get_context()->gid);

    return 0;
}

static int loggedFS_chmod(const char *path, mode_t mode)
{
    int res;

    res = chmod(getRelativePath(path).c_str(), mode);
    //loggedfs_log(aPath,"chmod",res,"chmod %s to %o",aPath, mode);
    if(res == -1)
        return -errno;

    return 0;
}

static char* getusername(uid_t uid)
{
struct passwd *p=getpwuid(uid);
if (p!=NULL)
	return p->pw_name;
return NULL;
}

static char* getgroupname(gid_t gid)
{
struct group *g=getgrgid(gid);
if (g!=NULL)
	return g->gr_name;
return NULL;
}

static int loggedFS_chown(const char *path, uid_t uid, gid_t gid)
{
    int res;

    res = lchown(getRelativePath(path).c_str(), uid, gid);

    char* username = getusername(uid);
    char* groupname = getgroupname(gid);
	
    if (username!=NULL && groupname!=NULL)
	    ;//loggedfs_log(aPath,"chown",res,"chown %s to %d:%d %s:%s",aPath, uid, gid, username, groupname);
    else
	   ;//loggedfs_log(aPath,"chown",res,"chown %s to %d:%d",aPath, uid, gid);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_truncate(const char *path, off_t size)
{
    int res;


    res = truncate(getRelativePath(path).c_str(), size);
    //loggedfs_log(aPath,"truncate",res,"truncate %s to %d bytes",aPath, size);
    if(res == -1)
        return -errno;

    return 0;
}

#if (FUSE_USE_VERSION==25)
static int loggedFS_utime(const char *path, struct utimbuf *buf)
{
    int res;

    res = utime(getRelativePath(path).c_str(), buf);
    //loggedfs_log(aPath,"utime",res,"utime %s",aPath);
    if(res == -1)
        return -errno;

    return 0;
}

#else


static int loggedFS_utimens(const char *path, const struct timespec ts[2])
{
    int res;
    struct timeval tv[2];

    tv[0].tv_sec = ts[0].tv_sec;
    tv[0].tv_usec = ts[0].tv_nsec / 1000;
    tv[1].tv_sec = ts[1].tv_sec;
    tv[1].tv_usec = ts[1].tv_nsec / 1000;

    res = utimes(getRelativePath(path).c_str(), tv);
    
    //loggedfs_log(aPath,"utimens",res,"utimens %s",aPath);
    if (res == -1)
        return -errno;

    return 0;
}

#endif








static int loggedFS_open(const char *path, struct fuse_file_info *fi)
{
    int res;

    res = open(getRelativePath(path).c_str(), fi->flags);

	// what type of open ? read, write, or read-write ?
	if (fi->flags & O_RDONLY) {
		 ;//loggedfs_log(aPath,"open-readonly",res,"open readonly %s",aPath);
	}
	else if (fi->flags & O_WRONLY) {
		 ;//loggedfs_log(aPath,"open-writeonly",res,"open writeonly %s",aPath);
	}
	else if (fi->flags & O_RDWR) {
		;//loggedfs_log(aPath,"open-readwrite",res,"open readwrite %s",aPath);
	}
	else  ;//loggedfs_log(aPath,"open",res,"open %s",aPath);
   
    if(res == -1)
        return -errno;

    fi->fh = res;
    return 0;
}

static int loggedFS_read(const char *path, char *buf, size_t size, off_t offset,
                         struct fuse_file_info *fi)
{
    int res;

    res = pread(fi->fh, buf, size, offset);

    if(res == -1)
        res = -errno;
    else ;//loggedfs_log(aPath,"read",0, "%d bytes read from %s at offset %d",res,aPath,offset);

    return res;
}

static int loggedFS_write(const char *path, const char *buf, size_t size,
                          off_t offset, struct fuse_file_info *fi)
{
    int res;

    res = pwrite(fi->fh, buf, size, offset);

    if(res == -1)
        res = -errno;
    else ;//loggedfs_log(aPath,"write",0, "%d bytes written to %s at offset %d",res,aPath,offset);

    return res;
}
static int loggedFS_read_buf(const char *path, struct fuse_bufvec **bufp,
                        size_t size, off_t offset, struct fuse_file_info *fi)
{
	try{
        struct fuse_bufvec *src;
        (void) path;
        src = new fuse_bufvec;
        //src = malloc(sizeof(struct fuse_bufvec));
        if (src == NULL)
                return -ENOMEM;
        *src = FUSE_BUFVEC_INIT(size);
        src->buf[0].flags = (fuse_buf_flags) (FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
        src->buf[0].fd = fi->fh;
        src->buf[0].pos = offset;
        *bufp = src;
        return 0;
	}
	catch(...){
		return -ENOMEM;
	}
}

static int loggedFS_write_buf(const char *path, struct fuse_bufvec *buf,
                     off_t offset, struct fuse_file_info *fi)
{
        struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));
        (void) path;
        dst.buf[0].flags = (fuse_buf_flags) (FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK);
        dst.buf[0].fd = fi->fh;
        dst.buf[0].pos = offset;
        return fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

static int loggedFS_flush(const char *path, struct fuse_file_info *fi)
{
        int res;
        (void) path;
        /* This is called from every close on an open file, so call the
           close on the underlying filesystem.  But since flush may be
           called multiple times for an open file, this must not really
           close the file.  This is important if used on a network
           filesystem like NFS which flush the data/metadata on close() */
        res = close(dup(fi->fh));
        if (res == -1)
                return -errno;
        return 0;
}

static int loggedFS_statfs(const char *path, struct statvfs *stbuf)
{
    int res;

    res = statvfs(getRelativePath(path).c_str(), stbuf);
    //loggedfs_log(aPath,"statfs",res,"statfs %s",aPath);
    if(res == -1)
        return -errno;

    return 0;
}

static int loggedFS_release(const char *path, struct fuse_file_info *fi)
{
    (void) path;
    close(fi->fh);
    return 0;
}

static int loggedFS_fsync(const char *path, int isdatasync,
                          struct fuse_file_info *fi)
{
	int res;
    (void) path;
	#ifndef HAVE_FDATASYNC
			(void) isdatasync;
	#else
			if (isdatasync)
					res = fdatasync(fi->fh);
			else
	#endif
					res = fsync(fi->fh);
	if (res == -1)
			return -errno;
	return 0;
}

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int loggedFS_setxattr(const char *path, const char *name, const char *value,
                             size_t size, int flags)
{
    int res = lsetxattr(getRelativePath(path).c_str(), name, value, size, flags);
    if(res == -1)
        return -errno;
    return 0;
}

static int loggedFS_getxattr(const char *path, const char *name, char *value,
                             size_t size)
{
    int res = lgetxattr(getRelativePath(path).c_str(), name, value, size);
    if(res == -1)
        return -errno;
    return res;
}

static int loggedFS_listxattr(const char *path, char *list, size_t size)
{
    int res = llistxattr(getRelativePath(path).c_str(), list, size);
    if(res == -1)
        return -errno;
    return res;
}

static int loggedFS_removexattr(const char *path, const char *name)
{
    int res = lremovexattr(getRelativePath(path).c_str(), name);
    if(res == -1)
        return -errno;
    return 0;
}
#endif /* HAVE_SETXATTR */


/*
static int loggedFS_lock(const char *path, struct fuse_file_info *fi, int cmd,
                    struct flock *lock)
{
        (void) path;
        return ulockmgr_op(fi->fh, cmd, lock, &fi->lock_owner,
                           (size_t)sizeof(fi->lock_owner));
}
*/
static int loggedFS_flock(const char *path, struct fuse_file_info *fi, int op)
{
        int res;
        (void) path;
        res = ::flock(fi->fh, op);
        if (res == -1)
                return -errno;
        return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int loggedFS_fallocate(const char *path, int mode,
                        off_t offset, off_t length, struct fuse_file_info *fi)
{
        (void) path;
        if (mode)
                return -EOPNOTSUPP;
        return -posix_fallocate(fi->fh, offset, length);
}
#endif

static void usage(char *name)
{
     fprintf(stderr, "Usage:\n");
     fprintf(stderr, "%s [-h] | [-l log-file] [-c config-file] [-f] [-p] [-e] /directory-mountpoint\n",name);
     fprintf(stderr, "Type 'man loggedfs' for more details\n");
     return;
}

static
bool processArgs(int argc, char *argv[], LoggedFS_Args *out)
{
    // set defaults
    out->isDaemon = true;

    out->fuseArgc = 0;
    out->configFilename.clear();

    // pass executable name through
    out->fuseArgv[0] = argv[0];
    ++out->fuseArgc;

    // leave a space for mount point, as FUSE expects the mount point before
    // any flags
    out->fuseArgv[1] = NULL;
    ++out->fuseArgc;
    opterr = 0;

    int res;

    bool got_p = false;

// We need the "nonempty" option to mount the directory in recent FUSE's
// because it's non empty and contains the files that will be logged.
//
// We need "use_ino" so the files will use their original inode numbers,
// instead of all getting 0xFFFFFFFF . For example, this is required for
// logging the ~/.kde/share/config directory, in which hard links for lock
// files are verified by their inode equivalency.

#define COMMON_OPTS "nonempty,use_ino"

    while ((res = getopt (argc, argv, "hpfec:l:")) != -1)
    {
        switch (res)
        {
	case 'h':
            usage(argv[0]);
            return false;
        case 'f':
            out->isDaemon = false;
            // this option was added in fuse 2.x
            PUSHARG("-f");
	    	rLog(Info,"LoggedFS not running as a daemon");
            break;
        case 'p':
            PUSHARG("-o");
            PUSHARG("allow_other,default_permissions," COMMON_OPTS);
            got_p = true; 
            rLog(Info,"LoggedFS running as a public filesystem");
            break;
        case 'e':
            PUSHARG("-o");
            PUSHARG("nonempty");
            rLog(Info,"Using existing directory");
            break;
        case 'c':
            out->configFilename=optarg;
	    	rLog(Info,"Configuration file : %s",optarg);
            break;
        case 'l':
        	logfilename = std::string(optarg);
            fileLog=open(logfilename.c_str(),O_WRONLY|O_CREAT|O_APPEND );
            fileLogNode=new StdioNode(fileLog);
            fileLogNode->subscribeTo( RLOG_CHANNEL("") );
	    	rLog(Info,"LoggedFS log file : %s",optarg);
            break;

        default:

            break;
        }
    }

    if (!got_p)
    {
        PUSHARG("-o");
        PUSHARG(COMMON_OPTS);
    }
#undef COMMON_OPTS

    if(optind+1 <= argc)
    {
        out->mountPoint = std::string(argv[optind++]);
        if(*out->mountPoint.rbegin()=='/')
        	out->mountPoint = out->mountPoint.substr(0, out->mountPoint.size()-1);
        out->fuseArgv[1] = out->mountPoint.c_str();
    }
    else
    {
        fprintf(stderr,"Missing mountpoint\n");
	usage(argv[0]);
        return false;
    }

    // If there are still extra unparsed arguments, pass them onto FUSE..
    if(optind < argc)
    {
        rAssert(out->fuseArgc < MaxFuseArgs);

        while(optind < argc)
        {
            rAssert(out->fuseArgc < MaxFuseArgs);
            out->fuseArgv[out->fuseArgc++] = argv[optind];
            ++optind;
        }
    }

    if(!isAbsolutePath( out->mountPoint ))
    {
        fprintf(stderr,"You must use absolute paths "
                "(beginning with '/') for %s\n",out->mountPoint.c_str());
        return false;
    }
    return true;
}

// using the sighup signal handler and reopen
// the logfile there is now a support for logrotate
void reopenLogFile (int param)
{
	if(fileLog==-1 || logfilename.length()<=0) return;

    int reopenfd = open(logfilename.c_str(), O_RDWR | O_APPEND | O_CREAT);
    if(reopenfd != -1){
    	dup2(reopenfd, fileLog);
    }
}

int main(int argc, char *argv[])
{
    RLogInit( argc, argv );

    StdioNode* stdLog=new StdioNode(STDOUT_FILENO);
    stdLog->subscribeTo( RLOG_CHANNEL("") );
    SyslogNode *logNode = NULL;

    signal (SIGHUP, reopenLogFile);


    umask(0);
    fuse_operations loggedFS_oper;
    // in case this code is compiled against a newer FUSE library and new
    // members have been added to fuse_operations, make sure they get set to
    // 0..
    std::fill((char*)&loggedFS_oper, ((char*)&loggedFS_oper) + sizeof(loggedFS_oper), '\0');
    loggedFS_oper.init		= loggedFS_init;
    loggedFS_oper.getattr	= loggedFS_getattr;
    loggedFS_oper.access	= loggedFS_access;
    loggedFS_oper.readlink	= loggedFS_readlink;
    loggedFS_oper.readdir	= loggedFS_readdir;
    loggedFS_oper.opendir        = loggedFS_opendir;
    loggedFS_oper.releasedir     = loggedFS_releasedir;
    loggedFS_oper.mknod	= loggedFS_mknod;
    loggedFS_oper.mkdir	= loggedFS_mkdir;
    loggedFS_oper.symlink	= loggedFS_symlink;
    loggedFS_oper.unlink	= loggedFS_unlink;
    loggedFS_oper.rmdir	= loggedFS_rmdir;
    loggedFS_oper.rename	= loggedFS_rename;
    loggedFS_oper.link	= loggedFS_link;
    loggedFS_oper.chmod	= loggedFS_chmod;
    loggedFS_oper.chown	= loggedFS_chown;
    loggedFS_oper.truncate	= loggedFS_truncate;
#if (FUSE_USE_VERSION==25)
    loggedFS_oper.utime       = loggedFS_utime;
#else
    loggedFS_oper.utimens	= loggedFS_utimens;
#endif
    loggedFS_oper.read_buf       = loggedFS_read_buf;
    loggedFS_oper.write_buf      = loggedFS_write_buf;
    loggedFS_oper.flush          = loggedFS_flush;
    loggedFS_oper.open	= loggedFS_open;
    loggedFS_oper.read	= loggedFS_read;
    loggedFS_oper.write	= loggedFS_write;
    loggedFS_oper.statfs	= loggedFS_statfs;
    loggedFS_oper.release	= loggedFS_release;
    loggedFS_oper.fsync	= loggedFS_fsync;

#ifdef HAVE_POSIX_FALLOCATE
    loggedFS_oper..fallocate      = loggedFS_fallocate,
#endif
#ifdef HAVE_SETXATTR
    loggedFS_oper.setxattr	= loggedFS_setxattr;
    loggedFS_oper.getxattr	= loggedFS_getxattr;
    loggedFS_oper.listxattr	= loggedFS_listxattr;
    loggedFS_oper.removexattr= loggedFS_removexattr;
#endif
    //loggedFS_oper.lock           = loggedFS_lock;
    loggedFS_oper.flock          = loggedFS_flock;

    for(int i=0; i<MaxFuseArgs; ++i)
        loggedfsArgs->fuseArgv[i] = NULL; // libfuse expects null args..

    if (processArgs(argc, argv, loggedfsArgs))
    {
        if (loggedfsArgs->isDaemon)
        {
            logNode = new SyslogNode( "loggedfs" );
            logNode->subscribeTo( RLOG_CHANNEL("") );
            // disable stderr reporting..
            delete stdLog;
            stdLog = NULL;
        }

        rLog(Info, "LoggedFS starting at %s.",loggedfsArgs->mountPoint.c_str());

        if(loggedfsArgs->configFilename.length()>0){
            if (loggedfsArgs->configFilename.compare("-")==0)
            {
                rLog(Info, "Using stdin configuration");
                std::string input;
                char c;
                while (cin.get(c))
                {
                   input += c;
                }

                config.loadFromXmlBuffer(input);
            }
            else
            {
                rLog(Info, "Using configuration file %s.",loggedfsArgs->configFilename.c_str());
                config.loadFromXmlFile(loggedfsArgs->configFilename);
            }
        }

        rLog(Info,"chdir to %s",loggedfsArgs->mountPoint.c_str());
        chdir(loggedfsArgs->mountPoint.c_str());
        savefd = open(".", 0);

#if (FUSE_USE_VERSION==25)
        fuse_main(loggedfsArgs->fuseArgc,
                  const_cast<char**>(loggedfsArgs->fuseArgv), &loggedFS_oper);
#else
	 fuse_main(loggedfsArgs->fuseArgc,
                  const_cast<char**>(loggedfsArgs->fuseArgv), &loggedFS_oper, NULL);
#endif
        delete stdLog;
        stdLog = NULL;
        if (fileLog!=-1)
        {
            delete fileLogNode;
            fileLogNode=NULL;
            close(fileLog);
        }
        if (loggedfsArgs->isDaemon)
        {
            delete logNode;
            logNode=NULL;
        }
        rLog(Info,"LoggedFS closing.");

    }
}
/*
 * missing ops
//int(* 	bmap )(const char *, size_t blocksize, uint64_t *idx)
//int(* 	ioctl )(const char *, int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data)
//int(* 	poll )(const char *, struct fuse_file_info *, struct fuse_pollhandle *ph, unsigned *reventsp)
 * */
