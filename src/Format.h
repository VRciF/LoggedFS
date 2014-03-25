#ifndef LOGGEDFS_FORMAT_H
#define LOGGEDFS_FORMAT_H

#include <unistd.h>

#include <algorithm>
#include <string>
#include <map>

class Format{
	public:
	    static std::string getDefaultFormatString();

		Format(std::string configformat=std::string());
		bool has(int type);
		void render(const std::map<int, std::string> &values, std::string &result);
		std::string getConfigFormat() { return this->configformat; }

	    static std::map<int, std::string> formatstrings;
	    enum Specifier {
	    	ACTION = 0,
	    	ERRNO,
	    	REQPID,
	    	REQUID,
	    	REQGID,
	    	REQUMASK,
	    	CMDNAME,

	    	UID,
	    	GID,
	    	USERNAME,
	    	GROUPNAME,

	    	ABSPATH,
	    	RELPATH,
	    	MODE,
	    	RDEV, // used in mknod

	    	ABSFROM,
	    	ABSTO,
	    	RELFROM,
	    	RELTO,

	    	FLAGS,
	    	ATIME,
	    	MTIME,
	    	OFFSET,
	    	SIZE,

	    	MODSECONDS,
	    	MODMICROSECONDS,
	    	ACSECONDS,
	    	ACMICROSECONDS,

	    	SECONDS,
	    	MICROSECONDS,

	    	TID,
	    	PID,
	    	PPID,

	    	XATTRNAME,
	    	XATTRVALUE,
	    	XATTRLIST
	    };

	protected:
	    static void init();

		std::string configformat;
		std::map<int, off_t> formatpositions;
		// offset -> pair(format type, length of format string)
		std::map<off_t, std::pair<int, int> > positionsformat;
};

#endif
