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
}

Filter::~Filter()
{
}

std::string Filter::getExtension() { return extension; }
int Filter::getUID() { return uid; }
std::string Filter::getAction() { return action; }
std::string Filter::getRetname() { return retname; }
Format& Filter::getFormat() { return format; }

void Filter::setExtension(const std::string e) { this->extension=e; }
void Filter::setUID(int u) { this->uid=u; }
void Filter::setAction(const std::string a) { this->action=a; }
void Filter::setRetname(const std::string r) { this->retname=r; }
void Filter::setFormat(const std::string f) { this->format=Format(f); }
void Filter::setFormat(const Format f) { this->format = f; }


bool Filter::matches( const std::string &str, const std::string &pattern)
{
    pcre *re;
    const char *error;
    int ovector[OVECCOUNT];
    int erroffset;


    re = pcre_compile(
             pattern.c_str(),
             0,
             &error,
             &erroffset,
             NULL);


    if (re == NULL)
    {
    	LOG_ERROR(Globals::instance()->errlog) << "PCRE compilation failed at offset " << erroffset << ": "<< error << std::endl;
        return false;
    }

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




bool Filter::matches(const std::string path,int uid, const std::string action, const std::string retname)
{
bool a= (matches(path,this->extension) && (uid==this->uid || this->uid==-1) && matches(action,this->action) && matches(retname,this->retname));

return a;
}
