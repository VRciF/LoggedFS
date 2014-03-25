/*****************************************************************************
 * Original Author:   Remi Flament <rflament at laposte dot net>
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

#include <limits.h>
#include <stdlib.h>

#include <sys/file.h>
#include <signal.h>

#include <iostream>
#include <sstream>
#include <map>

#include "Config.h"
#include "FSOperations.h"
#include "Util.h"
#include "Globals.h"

class LoggedFSRLogNode : public rlog::StdioNode{
    public:
	    LoggedFSRLogNode(int _fdOut = 2, int flags = (int)DefaultOutput)
			: StdioNode()
			, fdOut( _fdOut )
		{
			if(flags == DefaultOutput)
			flags = OutputColor | OutputContext;

		#ifdef USE_COLOURS
			colorize = (flags & OutputColor) && isatty( fdOut );
		#else
			colorize = false;
		#endif
			outputThreadId = (flags & OutputThreadId);
			outputContext   = (flags & OutputContext);
			outputChannel   = (flags & OutputChannel);
		}

	    LoggedFSRLogNode(int _fdOut, bool colorizeIfTTY)
			: StdioNode()
			, fdOut( _fdOut )
		{
		#ifdef USE_COLOURS
			colorize = colorizeIfTTY && isatty( fdOut );
		#else
			(void)colorizeIfTTY;
			colorize = false;
		#endif
			outputThreadId = false;
			outputContext  = true;
			outputChannel  = false;
		}

		virtual ~LoggedFSRLogNode()
		{
		}
	protected:
		virtual void publish( const rlog::RLogData &data )
		{
		#ifdef USE_STRSTREAM
			std::ostrstream ss;
		#else
			std::ostringstream ss;
		#endif

			ss << data.msg;
			ss << '\n';

			string out = ss.str();
			::write( fdOut, out.c_str(), out.length() );
		}

	    int fdOut;
};

static void usage(char *name)
{
     rError("Usage:\n");
     rError("%s [-h] | [-l log-file] [-c config-file] [-f] [-p] [-e] /directory-mountpoint\n",name);
     rError("Type 'man loggedfs' for more details\n");
     return;
}

static
bool processArgs(int argc, char *argv[], Globals::LoggedFS_Args *out)
{
    // set defaults
    out->isDaemon = true;

    out->configFilename.clear();

    // pass executable name through
    out->fuseArgv.push_back(argv[0]);

    // leave a space for mount point, as FUSE expects the mount point before
    // any flags
    out->fuseArgv.push_back(NULL);
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
            out->fuseArgv.push_back("-f");
	    	//rLog(Globals::instance()->Info,"LoggedFS not running as a daemon");
            break;
        case 'p':
            out->fuseArgv.push_back("-o");
            out->fuseArgv.push_back("allow_other,default_permissions," COMMON_OPTS);
            got_p = true; 
            //rLog(Globals::instance()->Info,"LoggedFS running as a public filesystem");
            break;
        case 'e':
            out->fuseArgv.push_back("-o");
            out->fuseArgv.push_back("nonempty");
            //rLog(Globals::instance()->Info,"Using existing directory");
            break;
        case 'c':
            out->configFilename=optarg;
	    	//rLog(Globals::instance()->Info,"Configuration file : %s",optarg);
            break;
        case 'l':
        	Globals::instance()->logfilename = std::string(optarg);
        	Globals::instance()->fileLog=open(Globals::instance()->logfilename.c_str(),O_WRONLY|O_CREAT|O_APPEND , S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
        	Globals::instance()->fileLogNode=new LoggedFSRLogNode(Globals::instance()->fileLog);
        	Globals::instance()->fileLogNode->subscribeTo( RLOG_CHANNEL("info") );
	    	//rLog(Globals::instance()->Info,"LoggedFS log file : %s",optarg);
            break;

        default:

            break;
        }
    }

    if (!got_p)
    {
        out->fuseArgv.push_back("-o");
        out->fuseArgv.push_back(COMMON_OPTS);
    }
#undef COMMON_OPTS

    if(optind+1 <= argc)
    {
        out->mountPoint = std::string(argv[optind++]);
        if(*out->mountPoint.rbegin()=='/')
        	out->mountPoint = out->mountPoint.substr(0, out->mountPoint.size()-1);
    }
    else
    {
        rError("Missing mountpoint");
	    usage(argv[0]);
        return false;
    }

    // If there are still extra unparsed arguments, pass them onto FUSE..
    if(optind < argc)
    {

        while(optind < argc)
        {
        	out->fuseArgv.push_back(argv[optind]);
            ++optind;
        }
    }

    if(out->mountPoint.length()<=0){
    	rError("invalid (none) mountpoint given");
	    usage(argv[0]);
        return false;
    }
    if(!Util::isAbsolutePath( out->mountPoint ))
    {
    	char *tmpc = getcwd(NULL, 0);
    	if(tmpc==NULL){
    		rError("Could not get current working directory (getcwd:%d)", errno);
    		return false;
    	}
    	std::string tmppath(tmpc);
    	free(tmpc);

    	tmppath += "/";
    	tmppath += out->mountPoint;

    	tmpc = realpath(tmppath.c_str() ,NULL);
    	if(tmpc==NULL){
    		rError("Could not get absolute mount point directory (realpath:%d) for %s", errno, out->mountPoint.c_str());
    		return false;
    	}
    	out->mountPoint = std::string(tmpc);
    	free(tmpc);
    }
    out->fuseArgv[1] = out->mountPoint.c_str();

    return true;
}

// using the sighup signal handler and reopen
// the logfile there is now a support for logrotate
void reopenLogFile (int param)
{
	if(Globals::instance()->fileLog==-1 || Globals::instance()->logfilename.length()<=0) return;

    int reopenfd = ::open(Globals::instance()->logfilename.c_str(), O_RDWR | O_APPEND | O_CREAT);
    if(reopenfd != -1){
    	::dup2(reopenfd, Globals::instance()->fileLog);
    }
}

int main(int argc, char *argv[])
{
	Globals::instance()->instance();
    rlog::RLogInit( argc, argv );

    rlog::StdioNode* stdLog = new LoggedFSRLogNode(STDOUT_FILENO);
    stdLog->subscribeTo( RLOG_CHANNEL("info") );
    rlog::SyslogNode *logNode = NULL;

    rlog::StdioNode* stdErr = new rlog::StdioNode(STDERR_FILENO);
    stdErr->subscribeTo( RLOG_CHANNEL("critical") );
    stdErr->subscribeTo( RLOG_CHANNEL("error") );
    stdErr->subscribeTo( RLOG_CHANNEL("warning") );
    stdErr->subscribeTo( RLOG_CHANNEL("notice") );
    stdErr->subscribeTo( RLOG_CHANNEL("debug") );

    signal (SIGHUP, reopenLogFile);


    umask(0);
    fuse_operations loggedFS_oper;
    // in case this code is compiled against a newer FUSE library and new
    // members have been added to fuse_operations, make sure they get set to
    // 0..
    std::fill((char*)&loggedFS_oper, ((char*)&loggedFS_oper) + sizeof(loggedFS_oper), '\0');
    loggedFS_oper.init		= FSOperations::init;
    loggedFS_oper.getattr	= FSOperations::getattr;
    loggedFS_oper.access	= FSOperations::access;
    loggedFS_oper.readlink	= FSOperations::readlink;
    loggedFS_oper.readdir	= FSOperations::readdir;
    loggedFS_oper.opendir        = FSOperations::opendir;
    loggedFS_oper.releasedir     = FSOperations::releasedir;
    loggedFS_oper.mknod	= FSOperations::mknod;
    loggedFS_oper.mkdir	= FSOperations::mkdir;
    loggedFS_oper.symlink	= FSOperations::symlink;
    loggedFS_oper.unlink	= FSOperations::unlink;
    loggedFS_oper.rmdir	= FSOperations::rmdir;
    loggedFS_oper.rename	= FSOperations::rename;
    loggedFS_oper.link	= FSOperations::link;
    loggedFS_oper.chmod	= FSOperations::chmod;
    loggedFS_oper.chown	= FSOperations::chown;
    loggedFS_oper.truncate	= FSOperations::truncate;
#if (FUSE_USE_VERSION==25)
    loggedFS_oper.utime       = FSOperations::utime;
#else
    loggedFS_oper.utimens	= FSOperations::utimens;
#endif
    loggedFS_oper.read_buf       = FSOperations::read_buf;
    loggedFS_oper.write_buf      = FSOperations::write_buf;
    loggedFS_oper.flush          = FSOperations::flush;
    loggedFS_oper.open	= FSOperations::open;
    loggedFS_oper.read	= FSOperations::read;
    loggedFS_oper.write	= FSOperations::write;
    loggedFS_oper.statfs	= FSOperations::statfs;
    loggedFS_oper.release	= FSOperations::release;
    loggedFS_oper.fsync	= FSOperations::fsync;

#ifdef HAVE_POSIX_FALLOCATE
    loggedFS_oper.fallocate      = FSOperations::fallocate,
#endif
#ifdef HAVE_SETXATTR
    loggedFS_oper.setxattr	= FSOperations::setxattr;
    loggedFS_oper.getxattr	= FSOperations::getxattr;
    loggedFS_oper.listxattr	= FSOperations::listxattr;
    loggedFS_oper.removexattr= FSOperations::removexattr;
#endif
    //loggedFS_oper.lock           = FSOperations::lock;
    loggedFS_oper.flock          = FSOperations::flock;


    if (processArgs(argc, argv, Globals::instance()->loggedfsArgs))
    {
        if (Globals::instance()->loggedfsArgs->isDaemon)
        {
        	logNode = new rlog::SyslogNode( "loggedfs" );
        	logNode->subscribeTo( RLOG_CHANNEL("critical") );
        	logNode->subscribeTo( RLOG_CHANNEL("error") );
        	logNode->subscribeTo( RLOG_CHANNEL("warning") );
        	logNode->subscribeTo( RLOG_CHANNEL("notice") );
        	logNode->subscribeTo( RLOG_CHANNEL("debug") );

        	if(Globals::instance()->fileLog==-1){
        		logNode->subscribeTo( RLOG_CHANNEL("info") );
        	}

            // disable stderr reporting..
            delete stdErr;
            stdErr = NULL;
    		delete stdLog;
    		stdLog = NULL;
        }

        //rLog(Globals::instance()->Info, "LoggedFS starting at %s.", Globals::instance()->loggedfsArgs->mountPoint.c_str());

        if(Globals::instance()->loggedfsArgs->configFilename.length()>0){
            if (Globals::instance()->loggedfsArgs->configFilename.compare("-")==0)
            {
                //rLog(Globals::instance()->Info, "Using stdin configuration");
                std::string input;
                char c;
                while (cin.get(c))
                {
                   input += c;
                }

                Globals::instance()->config.loadFromXmlBuffer(input);
            }
            else
            {
                //rLog(Globals::instance()->Info, "Using configuration file %s.",Globals::instance()->loggedfsArgs->configFilename.c_str());
                Globals::instance()->config.loadFromXmlFile(Globals::instance()->loggedfsArgs->configFilename);
            }
        }

        //rLog(Globals::instance()->Info,"chdir to %s",Globals::instance()->loggedfsArgs->mountPoint.c_str());
        ::chdir(Globals::instance()->loggedfsArgs->mountPoint.c_str());
        Globals::instance()->savefd = open(".", 0);

#if (FUSE_USE_VERSION<=25)
        fuse_main(Globals::instance()->loggedfsArgs->fuseArgv.size(),
                  const_cast<char**>(&Globals::instance()->loggedfsArgs->fuseArgv[0]), &loggedFS_oper);
#else
	 fuse_main(Globals::instance()->loggedfsArgs->fuseArgv.size(),
                  const_cast<char**>(&Globals::instance()->loggedfsArgs->fuseArgv[0]), &loggedFS_oper, NULL);
#endif
	    if(stdLog!=NULL)
	    {
            delete stdLog;
            stdLog = NULL;
        }
		if(stdErr!=NULL)
		{
			delete stdErr;
		    stdErr = NULL;
		}
        if (Globals::instance()->fileLog!=-1)
        {
            delete Globals::instance()->fileLogNode;
            Globals::instance()->fileLogNode=NULL;
            close(Globals::instance()->fileLog);
        }
        if (Globals::instance()->loggedfsArgs->isDaemon)
        {
            delete logNode;
            logNode=NULL;
        }
        //rLog(Globals::instance()->Info,"LoggedFS closing.");
    }
}
/*
 * missing ops
//int(* 	bmap )(const char *, size_t blocksize, uint64_t *idx)
//int(* 	ioctl )(const char *, int cmd, void *arg, struct fuse_file_info *, unsigned int flags, void *data)
//int(* 	poll )(const char *, struct fuse_file_info *, struct fuse_pollhandle *ph, unsigned *reventsp)
 * */
