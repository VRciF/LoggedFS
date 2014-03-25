#include "Format.h"

#include "Globals.h"

std::map<int, std::string> Format::formatstrings;

void Format::init(){
    if(Format::formatstrings.size()<=0){
    	Format::formatstrings[Format::ACTION] = "[action]";

    	Format::formatstrings[Format::ERRNO] = "[errno]";
    	Format::formatstrings[Format::REQPID] = "[reqpid]";
    	Format::formatstrings[Format::REQUID] = "[requid]";
    	Format::formatstrings[Format::REQGID] = "[reqgid]";
    	Format::formatstrings[Format::REQUMASK] = "[requmask]";

    	Format::formatstrings[Format::CMDNAME] = "[cmdname]";

    	Format::formatstrings[Format::ABSPATH] = "[abspath]";
    	Format::formatstrings[Format::RELPATH] = "[relpath]";

    	Format::formatstrings[Format::UID] = "[uid]";
    	Format::formatstrings[Format::GID] = "[gid]";
    	Format::formatstrings[Format::USERNAME] = "[username]";
    	Format::formatstrings[Format::GROUPNAME] = "[groupname]";
    	Format::formatstrings[Format::MODE] = "[mode]";
    	Format::formatstrings[Format::RDEV] = "[rdev]";
    	Format::formatstrings[Format::ABSFROM] = "[absfrom]";
    	Format::formatstrings[Format::ABSTO] = "[absto]";
    	Format::formatstrings[Format::RELFROM] = "[relfrom]";
    	Format::formatstrings[Format::RELTO] = "[relto]";
    	Format::formatstrings[Format::FLAGS] = "[flags]";
    	Format::formatstrings[Format::ATIME] = "[atime]";
    	Format::formatstrings[Format::MTIME] = "[mtime]";
    	Format::formatstrings[Format::OFFSET] = "[offset]";
    	Format::formatstrings[Format::SIZE] = "[size]";

    	Format::formatstrings[Format::MODSECONDS] = "[modseconds]";
    	Format::formatstrings[Format::MODMICROSECONDS] = "[modmicroseconds]";
    	Format::formatstrings[Format::ACSECONDS] = "[acseconds]";
    	Format::formatstrings[Format::ACMICROSECONDS] = "[acmicroseconds]";

    	Format::formatstrings[Format::SECONDS] = "[seconds]";
    	Format::formatstrings[Format::MICROSECONDS] = "[microseconds]";

    	Format::formatstrings[Format::TID] = "[tid]";
    	Format::formatstrings[Format::PID] = "[pid]";
    	Format::formatstrings[Format::PPID] = "[ppid]";

    	Format::formatstrings[Format::XATTRNAME] = "[xattrname]";
    	Format::formatstrings[Format::XATTRVALUE] = "[xattrvalue]";
    	Format::formatstrings[Format::XATTRLIST] = "[xattrlist]";
    }
}
std::string Format::getDefaultFormatString(){
	if(Format::formatstrings.size()<=0) Format::init();

    std::string df;

    // example: getattr /var/ {0} [ pid = 8700 ls uid = 1000 ]
    //          IACTION IABSPATH {IERRNO} [ pid = IREQPID ICMDNAME uid = IREQUID ]
    // [action] [abspath] {[errno]} [ pid = [reqpid] [cmdname] uid = [requid] ]

    df = Format::formatstrings[Format::ACTION];
    df += " "+Format::formatstrings[Format::ABSPATH];
    df += " {"+Format::formatstrings[Format::ERRNO]+"}";
    df += " [ pid = "+Format::formatstrings[Format::REQPID];
    df += " "+Format::formatstrings[Format::CMDNAME];
    df += " uid = "+Format::formatstrings[Format::REQUID]+" ]";

    return df;
}

Format::Format(std::string cf){
	if(Format::formatstrings.size()<=0) Format::init();
	if(cf.length()<=0) cf = Format::getDefaultFormatString();

	std::size_t pos;

	this->configformat = cf;

	std::string lower = this->configformat;
	std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    /*

	if((pos=lower.find(Format::formatstrings[Format::PID])) != std::string::npos){
		lower.replace(pos, Format::formatstrings[Format::PID].length(), "");
		this->configformat.replace(pos,
				                   Format::formatstrings[Format::PID].length(),
				                   ((std::stringstream&)(std::stringstream() << getpid() )).str()
				                  );
	}
	if((pos=lower.find(Format::formatstrings[Format::PPID])) != std::string::npos){
		lower.replace(pos, Format::formatstrings[Format::PPID].length(), "");
		this->configformat.replace(pos,
				                   Format::formatstrings[Format::PPID].length(),
				                   ((std::stringstream&)(std::stringstream() << getppid() )).str()
				                  );
	}
	*/

	// analyze config format
	std::map<int, std::string>::iterator fsiter = Format::formatstrings.begin();
	for(;fsiter!=Format::formatstrings.end();++fsiter){
		pos = lower.find(fsiter->second);
		if(pos!=std::string::npos){
			formatpositions[fsiter->first] = pos;
			positionsformat[pos] = std::make_pair(fsiter->first, fsiter->second.length());
		}
	}
}

bool Format::has(int type){
	return this->formatpositions.find(type)!=this->formatpositions.end();
}

void Format::render(const std::map<int, std::string> &values, std::string &result){
	result.clear();

	std::map<int, std::string>::const_iterator cvalit;
	std::map<off_t, std::pair<int,int> >::iterator fpit = positionsformat.begin();
	size_t prevconfigformatpos = 0;
	for(;fpit!=positionsformat.end();++fpit){
		cvalit = values.find(fpit->second.first);
		if(cvalit==values.end())
			continue;

		result.append(this->configformat.substr(prevconfigformatpos, fpit->first - prevconfigformatpos));
		result.append(cvalit->second);
		prevconfigformatpos = fpit->first + fpit->second.second;
	}
	result.append(this->configformat.substr(prevconfigformatpos));
}
