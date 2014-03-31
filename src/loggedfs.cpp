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
#include <ext/stdio_filebuf.h>

#include "cpplog/cpplog.hpp"

#include "Config.h"
#include "FSOperations.h"
#include "Util.h"
#include "Globals.h"

static void usage(char *name)
{
	LOG_ERROR(Globals::instance()->errlog) << "Usage:" << std::endl;
	LOG_ERROR(Globals::instance()->errlog) << name << " [-h] | [-l log-file] [-c config-file] [-f] [-p] [-e] [/]directory-mountpoint" << std::endl;
     //LOG_ERROR(Globals::instance()->errlog) << "Type 'man loggedfs' for more details" << std::endl;
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
	    	LOG_INFO(Globals::instance()->errlog) << "LoggedFS not running as a daemon" << std::endl;
            break;
        case 'p':
            out->fuseArgv.push_back("-o");
            out->fuseArgv.push_back("allow_other,default_permissions," COMMON_OPTS);
            got_p = true; 
            LOG_INFO(Globals::instance()->errlog) << "LoggedFS running as a public filesystem" << std::endl;
            break;
        case 'e':
            out->fuseArgv.push_back("-o");
            out->fuseArgv.push_back("nonempty");
            LOG_INFO(Globals::instance()->errlog) << "Using existing directory" << std::endl;
            break;
        case 'c':
            out->configFilename=optarg;
            LOG_INFO(Globals::instance()->errlog) << "Configuration file : " << optarg << std::endl;
            break;
        case 'l':
            {
				Globals::instance()->logfilename = std::string(optarg);
				int fd = open(Globals::instance()->logfilename.c_str(), O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
				if(fd == -1){
					LOG_ERROR(Globals::instance()->errlog) << "Could not open log file: "<< optarg << "- {"
							                               << errno << ":"<< strerror(errno) << "}" << std::endl;
					return false;
				}

				__gnu_cxx::stdio_filebuf<char> *fb = new __gnu_cxx::stdio_filebuf<char>(fd, std::ios::out);
				if(Globals::instance()->filebuf != NULL){
					delete Globals::instance()->filebuf;
				}
				Globals::instance()->filebuf = fb;

				if(Globals::instance()->logger!=NULL){
					delete Globals::instance()->logger;
				}
				std::ostream *os = new std::ostream(Globals::instance()->filebuf);
				Globals::instance()->logger = new cpplog::OstreamLogger(*os);
            }
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
    	LOG_ERROR(Globals::instance()->errlog) << "Missing mountpoint" << std::endl;
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
    	LOG_ERROR(Globals::instance()->errlog) << "invalid (none) mountpoint given" << std::endl;
	    usage(argv[0]);
        return false;
    }
    if(!Util::isAbsolutePath( out->mountPoint ))
    {
    	char *tmpc = getcwd(NULL, 0);
    	if(tmpc==NULL){
    		LOG_ERROR(Globals::instance()->errlog) << "Could not get current working directory (getcwd:"<<errno<<")" << std::endl;
    		return false;
    	}
    	std::string tmppath(tmpc);
    	free(tmpc);

    	tmppath += "/";
    	tmppath += out->mountPoint;

    	tmpc = realpath(tmppath.c_str() ,NULL);
    	if(tmpc==NULL){
    		LOG_ERROR(Globals::instance()->errlog) << "Could not get absolute mount point directory (realpath:"<< errno <<") for "
    				                               << out->mountPoint.c_str() << std::endl;
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
	if(Globals::instance()->filebuf==NULL || !Globals::instance()->filebuf->is_open() || Globals::instance()->logfilename.length()<=0) return;

    int reopenfd = ::open(Globals::instance()->logfilename.c_str(), O_WRONLY|O_CREAT|O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP);
    if(reopenfd != -1){
    	::dup2(reopenfd, Globals::instance()->filebuf->fd());
    }
}

int main(int argc, char *argv[])
{
	Globals::instance()->instance();

    signal (SIGHUP, reopenLogFile);

    umask(0);

    if (processArgs(argc, argv, Globals::instance()->loggedfsArgs))
    {
    	LOG_INFO(Globals::instance()->errlog) << "LoggedFS starting at " << Globals::instance()->loggedfsArgs->mountPoint.c_str() << std::endl;

        if(Globals::instance()->loggedfsArgs->configFilename.length()>0){
            if (Globals::instance()->loggedfsArgs->configFilename.compare("-")==0)
            {
            	LOG_INFO(Globals::instance()->errlog) << "Using stdin configuration" << std::endl;
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
            	LOG_INFO(Globals::instance()->errlog) <<  "Using configuration file " << Globals::instance()->loggedfsArgs->configFilename.c_str() << std::endl;
                Globals::instance()->config.loadFromXmlFile(Globals::instance()->loggedfsArgs->configFilename);
            }
        }

        LOG_INFO(Globals::instance()->errlog) << "chdir to " << Globals::instance()->loggedfsArgs->mountPoint.c_str() << std::endl;
        ::chdir(Globals::instance()->loggedfsArgs->mountPoint.c_str());
        Globals::instance()->savefd = open(".", 0);

#if (FUSE_USE_VERSION<=25)
        fuse_main(Globals::instance()->loggedfsArgs->fuseArgv.size(),
                  const_cast<char**>(&Globals::instance()->loggedfsArgs->fuseArgv[0]), &FSOperations::getFuseOperations());
#else
	 fuse_main(Globals::instance()->loggedfsArgs->fuseArgv.size(),
                  const_cast<char**>(&Globals::instance()->loggedfsArgs->fuseArgv[0]), &FSOperations::getFuseOperations(), NULL);
#endif
        if (Globals::instance()->filebuf!=NULL)
        {
        	if(Globals::instance()->filebuf->is_open())
        	    Globals::instance()->filebuf->close();
        	delete Globals::instance()->filebuf;
        }
        if (Globals::instance()->logger!=NULL)
        {
        	delete Globals::instance()->logger;
        	Globals::instance()->logger = NULL;
        }
    }
}
