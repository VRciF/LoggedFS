#ifndef LOGGEDFS_GLOBALS_H
#define LOGGEDFS_GLOBALS_H

#include <vector>
#include <ext/stdio_filebuf.h>

#include "Config.h"
#include "Format.h"

#include "cpplog/cpplog.hpp"

#define LOGGEDFS_ERROR(logger) cpplog::LogMessage(__FILE__, __LINE__, (LL_ERROR),(logger), false).getStream()
#define LOGGEDFS_INFO(logger) cpplog::LogMessage(__FILE__, __LINE__, (LL_INFO),(logger), false).getStream()

class Globals{
public:
	struct LoggedFS_Args
	{
		std::string mountPoint; // where the users read files
		std::string configFilename;
		bool isDaemon; // true == spawn in background, log to syslog
		std::vector<const char*> fuseArgv;
	};

	LoggedFS_Args *loggedfsArgs;

	Config config;
	int savefd;
	std::string logfilename;
	__gnu_cxx::stdio_filebuf<char> *filebuf;

	cpplog::BaseLogger *logger;
	cpplog::StdErrLogger errlog;

	static Globals* instance(){
		static Globals *i = NULL;
		if(i==NULL){
			i = new Globals();

			i->filebuf = NULL;
			i->loggedfsArgs = new LoggedFS_Args;
			i->logger = new cpplog::OstreamLogger(std::cout);
		}
		return i;
	}
};

#endif
