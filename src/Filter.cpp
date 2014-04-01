/*****************************************************************************
 * Original Author:   Remi Flament <rflament at laposte dot net>
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
#include "Filter.h"
#include <pcre.h>
#include <sys/types.h>
#include <unistd.h>

#include <string>
#include <sstream>

#include "Globals.h"

#define OVECCOUNT 30

Filter::Filter()
{
	this->regex_extension = NULL;
	this->regex_action = NULL;
	this->regex_retname = NULL;
}

Filter::~Filter()
{
	if(this->regex_extension!=NULL) pcre_free(this->regex_extension);
	if(this->regex_action!=NULL) pcre_free(this->regex_action);
	if(this->regex_retname!=NULL) pcre_free(this->regex_retname);
}

std::string Filter::getExtension() { return extension; }
int Filter::getUID() { return uid; }
std::string Filter::getAction() { return action; }
std::string Filter::getRetname() { return retname; }
Format& Filter::getFormat() { return format; }

void Filter::setExtension(const std::string e) {
	this->extension=e;
	if(this->regex_extension!=NULL) pcre_free(this->regex_extension);
	this->regex_extension = this->compileRegex(this->extension);
}
void Filter::setUID(int u) { this->uid=u; }
void Filter::setAction(const std::string a) {
	this->action=a;
	if(this->regex_action!=NULL) pcre_free(this->regex_action);
	this->regex_action = this->compileRegex(this->action);
}
void Filter::setRetname(const std::string r) {
	this->retname=r;
	if(this->regex_retname!=NULL) pcre_free(this->regex_retname);
	this->regex_retname = this->compileRegex(this->retname);
}
void Filter::setFormat(const std::string f) { this->format=Format(f); }
void Filter::setFormat(const Format f) { this->format = f; }

//bool Filter::matches( const std::string &str, const std::string &pattern)

pcre* Filter::compileRegex(const std::string &pattern){
	pcre *re = NULL;
    const char *error;
    int erroffset;

	re = pcre_compile(
			 pattern.c_str(),
			 0,
			 &error,
			 &erroffset,
			 NULL);

	if (re == NULL)
	{
		LOGGEDFS_ERROR(Globals::instance()->errlog) << "PCRE compilation failed at offset " << erroffset << ": "<< error << std::endl;
	}
	return re;
}

bool Filter::matches( const std::string &str, pcre *re)
{
	if(re==NULL){
		return false;
	}

    int ovector[OVECCOUNT];
    int rc = pcre_exec(
                 re,                   /* the compiled pattern */
                 NULL,                 /* no extra data - we didn't study the pattern */
                 str.c_str(),              /* the subject string */
                 str.length(),       /* the length of the subject */
                 0,                    /* start at offset 0 in the subject */
                 0,                    /* default options */
                 ovector,              /* output vector for substring information */
                 OVECCOUNT);           /* number of elements in the output vector */

    return (rc >= 0);
}

bool Filter::matchPath(const std::string path){
	return matches(path,this->regex_extension);
}
bool Filter::matchUid(int uid){
	return (uid==this->uid || this->uid==-1);
}
bool Filter::matchAction(const std::string action){
	return matches(action,this->regex_action);
}
bool Filter::matchRetname(const std::string retname){
	return matches(retname,this->regex_retname);
}

bool Filter::matches(const std::string path,int uid, const std::string action, const std::string retname)
{
    bool a = (this->matchPath(path) && this->matchUid(uid) && this->matchAction(action) && this->matchRetname(retname));
    return a;
}
