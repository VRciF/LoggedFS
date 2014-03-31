#ifndef LOGGEDFS_GLOBALS_H
#define LOGGEDFS_GLOBALS_H

#include <vector>

#include "Config.h"
#include "Format.h"

#include "cpplog/cpplog.hpp"
/*
class NullLogger : public cpplog::BaseLogger
{
public:
	NullLogger()
	{ }

	virtual bool sendLogMessage(cpplog::LogData* logData)
	{
		return true;
	}

	virtual ~NullLogger() { }
};
*/

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
