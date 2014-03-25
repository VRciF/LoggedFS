#ifndef LOGGEDFS_GLOBALS_H
#define LOGGEDFS_GLOBALS_H

#include <rlog/rlog.h>
#include <rlog/Error.h>
#include <rlog/RLogChannel.h>
#include <rlog/SyslogNode.h>
#include <rlog/StdioNode.h>

#include <vector>

#include "Config.h"
#include "Format.h"

class Globals{
public:
	struct LoggedFS_Args
	{
		std::string mountPoint; // where the users read files
		std::string configFilename;
		bool isDaemon; // true == spawn in background, log to syslog
		std::vector<const char*> fuseArgv;

		//const char *fuseArgv[MaxFuseArgs];
		//int fuseArgc;
	};

	LoggedFS_Args *loggedfsArgs;

	rlog::RLogChannel *Info;
	rlog::StdioNode* fileLogNode;

	Config config;
	int savefd;
	std::string logfilename;
	int fileLog;

	//const int MaxFuseArgs;

	static Globals* instance(){
		static Globals *i = NULL;
		if(i==NULL){
			i = new Globals();

			i->loggedfsArgs = new LoggedFS_Args;
			i->Info = DEF_CHANNEL("info", rlog::Log_Info);
			i->fileLog = -1;
			i->fileLogNode = NULL;
			//i->MaxFuseArgs = 32;
		}
		return i;
	}
};

#endif
