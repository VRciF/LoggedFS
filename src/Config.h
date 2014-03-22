/*****************************************************************************
 * Author:   Rémi Flament <remipouak@yahoo.fr>
 *
 *****************************************************************************
 * Copyright (c) 2005, Rémi Flament
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

#ifndef LOGGEDFS_CONFIG_H
#define LOGGEDFS_CONFIG_H

using namespace std;
#include <vector>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include <map>
#include <string>

#include "Filter.h"

class Config
{
public:
    Config();
    ~Config();

    bool load(const char *fileName);
    bool loadFromXmlFile(const char *fileName);
    bool loadFromXmlBuffer(const char *buffer);
    bool loadFromXml(xmlDoc* doc);
    bool isEnabled() {return enabled;};
    bool isTimeEnabled() {return timeEnabled;};
    bool isPrintProcessNameEnabled() {return pNameEnabled;};
    bool shouldLog(const char* filename, int uid, const char* action, const char* retname, char **format);
    char* toString();

    static std::map<int, std::string>* formatstrings();

    enum Format {
    	FORMAT_ACTION = 0,
    	FORMAT_ERRNO,
    	FORMAT_REQPID,
    	FORMAT_REQUID,
    	FORMAT_REQGID,
    	FORMAT_REQUMASK,
    	FORMAT_CMDNAME,

    	FORMAT_UID,
    	FORMAT_GID,
    	FORMAT_USERNAME,
    	FORMAT_GROUPNAME,

    	FORMAT_ABSPATH,
    	FORMAT_RELPATH,
    	FORMAT_MODE,
    	FORMAT_RDEV, // used in mknod

    	FORMAT_FROMABSPATH,
    	FORMAT_TOABSPATH,
    	FORMAT_FROMRELPATH,
    	FORMAT_TORELPATH,

    	FORMAT_FLAGS,
    	FORMAT_ATIME,
    	FORMAT_MTIME,
    	FORMAT_OFFSET,
    	FORMAT_SIZE,

    	FORMAT_XATTRNAME,
    	FORMAT_XATTRVALUE,
    	FORMAT_XATTRLIST,
    };

private:
    void parse(xmlNode*);
    std::vector<Filter> includes;
    std::vector<Filter> excludes;
    bool enabled;
    bool timeEnabled;
    bool pNameEnabled;

    std::string defaultformat;
};

#endif
