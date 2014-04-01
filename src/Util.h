#ifndef LOGGEDFS_UTIL_H
#define LOGGEDFS_UTIL_H

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>
#include <sys/time.h>

#include <fstream>
#include <iterator>

#include "Globals.h"

class Util{
  public:
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

		realPath += Globals::instance()->loggedfsArgs->mountPoint;
		realPath += path;

		return realPath;
	}

	static std::string getRelativePath(const char *path)
	{
		std::string rPath;
		if(path==NULL) return rPath;

		rPath += ".";
		rPath += path;

		return rPath;
	}


	static std::string getusername(uid_t uid)
	{
		struct passwd *p=getpwuid(uid);
		if (p!=NULL)
			return p->pw_name;
		return "";
	}

	static std::string getgroupname(gid_t gid)
	{
		struct group *g=getgrgid(gid);
		if (g!=NULL)
			return g->gr_name;
		return "";
	}


	/*
	 * Returns the name of the process which accessed the file system.
	 */
	static std::string getcallername()
	{
		std::stringstream ss;
		ss << "/proc/" << fuse_get_context()->pid << "/cmdline";
		std::string filename = ss.str();

		std::ifstream infile(filename.c_str());
		char cmdline[8192];
		infile.getline(cmdline, sizeof(cmdline));

		return std::string(cmdline);
	}

	static void log(LogMap &values)
	{
		Util::log(values.values);
	}
	static void log(std::map<int, std::string> &values)
	{
		std::map<int, std::string>::const_iterator iter;

		const char *action,*path,*retname;

		iter = values.find(Format::ACTION);
		if(iter==values.end()) return;
		action = iter->second.c_str();

		iter = values.find(Format::RELPATH);
		if(iter==values.end()) return;
		path = iter->second.c_str();

		iter = values.find(Format::ERRNO);
		if(iter==values.end())
			iter = values.insert(values.end(),
					             std::make_pair(Format::ERRNO, ((std::stringstream&)(std::stringstream() << errno)).str())
		    );
		retname = iter->second.compare("0")==0?"SUCCESS":"FAILURE";

		Format *f;
		if (Globals::instance()->config.shouldLog(path,fuse_get_context()->uid,action,retname, &f)){
			struct fuse_context *ctx = fuse_get_context();

			if(f->has(Format::ABSPATH) && values.find(Format::ABSPATH)==values.end())
				values[Format::ABSPATH] = Util::getAbsolutePath(path);
			if(f->has(Format::CMDNAME) && values.find(Format::CMDNAME)==values.end())
			    values[Format::CMDNAME] = Util::getcallername();
			if(f->has(Format::SECONDS) || f->has(Format::MICROSECONDS)){
				struct timeval tv;
				gettimeofday(&tv, NULL);
				values[Format::SECONDS] = ((std::stringstream&)(std::stringstream() << tv.tv_sec)).str();
				values[Format::MICROSECONDS] = ((std::stringstream&)(std::stringstream() << tv.tv_usec)).str();
			}

			if(f->has(Format::TID)){
				values[Format::REQPID] = ((std::stringstream&)(std::stringstream() << pthread_self())).str();
			}

			if(ctx && f->has(Format::REQPID) && values.find(Format::REQPID)==values.end())
				values[Format::REQPID] = ((std::stringstream&)(std::stringstream() << ctx->pid)).str();
			if(ctx && f->has(Format::REQUID) && values.find(Format::REQUID)==values.end())
				values[Format::REQUID] = ((std::stringstream&)(std::stringstream() << ctx->uid)).str();
			if(ctx && f->has(Format::REQGID) && values.find(Format::REQGID)==values.end())
				values[Format::REQGID] = ((std::stringstream&)(std::stringstream() << ctx->gid)).str();
			if(ctx && f->has(Format::REQUMASK) && values.find(Format::REQUMASK)==values.end())
				values[Format::REQUMASK] = ((std::stringstream&)(std::stringstream() << ctx->umask)).str();

			if(values.find(Format::UID)!=values.end())
				values[Format::USERNAME] = Util::getusername(atoi(values[Format::UID].c_str()));
			if(values.find(Format::GID)!=values.end())
				values[Format::GROUPNAME] = Util::getgroupname(atoi(values[Format::GID].c_str()));

			std::string message;
			f->render(values, message);

			LOGGEDFS_INFO(*Globals::instance()->logger) << message << std::endl;
	    }
	}
	template <class OutIt>
	static void explode(OutIt output, const std::string& str, const std::string delimiters, int limit=0) {
		int factor = 0;
		if(limit != 0)
			factor = limit > 0 ? -1 : 1;

	    std::vector<std::string> tokens;

	    std::size_t subStrBeginPos = str.find_first_not_of(delimiters, 0);
	    std::size_t subStrEndPos = str.find_first_of(delimiters, subStrBeginPos);

	    while(std::string::npos != subStrBeginPos || std::string::npos != subStrEndPos)
	    {
	    	if(limit==1)
	    		*output++ = str.substr(subStrBeginPos);
	    	else
	    	    *output++ = str.substr(subStrBeginPos, subStrEndPos-subStrBeginPos);

	        subStrBeginPos = str.find_first_not_of(delimiters, subStrEndPos);
	        subStrEndPos = str.find_first_of(delimiters, subStrBeginPos);

	    	if(factor!=0 && limit!=0)
	    		limit += factor;
	    	if(factor!=0 && limit==0)
	    		break;
	    }
	}
	static void explode(std::vector<std::string> &tokens, const std::string& str, const std::string delimiters = " ", int limit=0)
	{
	    Util::explode<std::back_insert_iterator< std::vector<std::string> > >(std::back_insert_iterator< std::vector<std::string> >(tokens),
	    		                                                              str, delimiters, limit);
	    /*
	    auto subStrBeginPos = str.find_first_not_of(delimiters, 0);
	    auto subStrEndPos = str.find_first_of(delimiters, subStrBeginPos);

	    while(std::string::npos != subStrBeginPos || std::string::npos != subStrEndPos)
	    {
	        tokens.push_back(str.substr(subStrBeginPos, subStrEndPos-subStrBeginPos));

	        subStrBeginPos = str.find_first_not_of(delimiters, subStrEndPos);
	        subStrEndPos = str.find_first_of(delimiters, subStrBeginPos);
	    }
	    */
	}

	static void parseStyleSheetFormat(std::map<std::string,std::string> keyvalue, const std::string& str){
		std::vector<std::string> parts;
		Util::explode(parts, str, ";");
		std::vector<std::string>::iterator it;
		for(it=parts.begin();it!=parts.end();++it){
			std::vector<std::string> tuple;
			Util::explode(tuple, *it, ":", 2);
			if(tuple.size() <= 0)
				continue;

			std::string key, value;
			if(tuple.size()>0)
				key = tuple[0];
			if(tuple.size()>1)
				value = tuple[1];
			keyvalue[key] = value;
		}
	}

};

#endif

