/*****************************************************************************
 * Author:   Remi Flament <rflament at laposte dot net>
 *****************************************************************************
 * Copyright (c) 2005 - 2007, Remi Flament
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
#ifndef LOGGEDFS_FILTER_H
#define LOGGEDFS_FILTER_H

#include <string>

class Filter
{
public:
	Filter();
	~Filter();
	std::string getExtension() {return extension;};
	int getUID() {return uid;};
	std::string getAction() {return action;};
	std::string getRetname() {return retname;};
	std::string getFormat() {return format;};
	
	void setExtension(const std::string e) {this->extension=e;};
	void setUID(int u) {this->uid=u;};
	void setAction(const std::string a) {this->action=a;};
	void setRetname(const std::string r) {this->retname=r;};
	void setFormat(const std::string f) {this->format=f;};
	bool matches(const std::string path, int uid, const std::string action, const std::string retname);
	
private:
	std::string extension;
	int uid;
	std::string action;
	std::string retname;
	std::string format;
	bool matches( const std::string &str,const std::string &pattern);
};


#endif
