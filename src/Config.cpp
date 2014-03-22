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

#include "Config.h"
#include <fstream>
#include <iostream>




xmlChar* INCLUDE=xmlCharStrdup("include");
xmlChar* EXCLUDE=xmlCharStrdup("exclude");
xmlChar* USER=xmlCharStrdup("uid");
xmlChar* EXTENSION=xmlCharStrdup("extension");
xmlChar* ACTION=xmlCharStrdup("action");
xmlChar* RETNAME=xmlCharStrdup("retname");
xmlChar* ROOT=xmlCharStrdup("loggedFS");
xmlChar* LOG_ENABLED=xmlCharStrdup("logEnabled");
xmlChar* PNAME_ENABLED=xmlCharStrdup("printProcessName");
xmlChar* FORMAT=xmlCharStrdup("format");

Config::Config()
{
    // default values
    enabled=true; 
    pNameEnabled=true;

    std::map<int, std::string> *fstr = Config::formatstrings();
    // example: getattr /var/ {0} [ pid = 8700 ls uid = 1000 ]
    //          IACTION IABSPATH {IERRNO} [ pid = IREQPID ICMDNAME uid = IREQUID ]
    defaultformat = (*fstr)[Config::FORMAT_ACTION];
    defaultformat += " "+(*fstr)[Config::FORMAT_ABSPATH];
    defaultformat += " {"+(*fstr)[Config::FORMAT_ERRNO]+"}";
    defaultformat += "[ pid = "+(*fstr)[Config::FORMAT_REQPID]+" "+(*fstr)[Config::FORMAT_CMDNAME]+" uid = "+(*fstr)[Config::FORMAT_REQUID]+" ]";
}

Config::~Config()
{
    includes.clear();
    excludes.clear();
}

void Config::parse(xmlNode * a_node)
{

xmlNode *cur_node = NULL;

for (cur_node = a_node; cur_node; cur_node = cur_node->next) 
	{
	if (cur_node->type == XML_ELEMENT_NODE) 
		{
		xmlAttr *attr=cur_node->properties;
		if (xmlStrcmp(cur_node->name,ROOT)==0)
			{
			while (attr!=NULL)
				{
				if (xmlStrcmp(attr->name,LOG_ENABLED)==0)
					{
					//enable or disable loggedfs
					if (xmlStrcmp(attr->children->content,xmlCharStrdup("true"))!=0)
						{
						enabled=false;
						printf("Log disabled\n");
						}
					else	{
						printf("Log enabled\n");
						}
					}
				else if (xmlStrcmp(attr->name,PNAME_ENABLED)==0)
					{
					//enable or disable process name prints in loggedfs
					if (xmlStrcmp(attr->children->content,xmlCharStrdup("true"))!=0)
						{
						pNameEnabled=false;
						printf("print process name disabled\n");
						}
					else	{
						printf("print process name enabled\n");
						}
					}
				else printf("unknown attribute : %s\n",attr->name);
				attr=attr->next;
				}
			}
		if (xmlStrcmp(cur_node->name,INCLUDE)==0 || xmlStrcmp(cur_node->name,EXCLUDE)==0)
			{
			Filter* filter=new Filter();
			std::string buffer;
			while (attr!=NULL)
				{
				buffer = std::string((char*)attr->children->content);

				if (xmlStrcmp(attr->name,EXTENSION)==0)
					{
					filter->setExtension(buffer);
					}
				else if (xmlStrcmp(attr->name,USER)==0)
					{
					if (buffer.compare("*")!=0)
						filter->setUID(atoi(buffer.c_str()));
					else filter->setUID(-1); // every users

					}
				else if (xmlStrcmp(attr->name,ACTION)==0)
					{
					filter->setAction(buffer);
					}
				else if (xmlStrcmp(attr->name,RETNAME)==0)
					{
					filter->setRetname(buffer);
					}
				else if (xmlStrcmp(attr->name,FORMAT)==0)
					{
					filter->setFormat(buffer);
					}
				else printf("unknown attribute : %s\n",attr->name);
				attr=attr->next;
				}
			
			if (xmlStrcmp(cur_node->name,INCLUDE)==0)
				{
				includes.push_back(*filter);
				}
			else excludes.push_back(*filter);
			}		
    		}
		

   	parse(cur_node->children);
	}

}

bool Config::loadFromXml(xmlDoc* doc)
{
	
	xmlNode *root_element = NULL;
	root_element = xmlDocGetRootElement(doc);
	
	parse(root_element);
	xmlFreeDoc(doc);
	xmlCleanupParser();
	return true;
}

bool Config::loadFromXmlBuffer(const std::string buffer)
{
	xmlDoc *doc = NULL;

	LIBXML_TEST_VERSION
	
	doc=xmlReadMemory(buffer.c_str(),buffer.length(),"",NULL, XML_PARSE_NONET);
	return loadFromXml(doc);
}

bool Config::loadFromXmlFile(const std::string filename)
{
	xmlDoc *doc = NULL;

	LIBXML_TEST_VERSION
	
	doc = xmlReadFile(filename.c_str(), NULL, 0);
	return loadFromXml(doc);
}

bool Config::shouldLog(const char* filename, int uid, const char* action, const char* retname, std::string &format)
{
    bool should=false;

    if (enabled)
    {
    	if (includes.size()>0)
		{
			for (unsigned int i=0;i<includes.size() && !should;i++)
			{
			Filter f=includes[i];
			if (f.matches(filename,uid,action,retname))
				format.replace(format.begin(),format.end(), f.getFormat());
				should=true;
			}
			for (unsigned int i=0;i<excludes.size() && should;i++)
			{
			Filter f=excludes[i];
			if (f.matches(filename,uid,action,retname))
				should=false;
			}
		}
		else
		{
			should=true;
		}

    }
    
    return should;
}

std::map<int, std::string>* Config::formatstrings(){
	static std::map<int, std::string> formatstrings;
	if(formatstrings.size()<=0){
		formatstrings[Config::FORMAT_ACTION] = "[action]";
		formatstrings[Config::FORMAT_ERRNO] = "[errno]";
		formatstrings[Config::FORMAT_REQPID] = "[reqpid]";
		formatstrings[Config::FORMAT_REQUID] = "[requid]";
		formatstrings[Config::FORMAT_REQGID] = "[reqgid]";
		formatstrings[Config::FORMAT_REQUMASK] = "[requmask]";
		formatstrings[Config::FORMAT_CMDNAME] = "[cmdname]";
	}
	return &formatstrings;
}

