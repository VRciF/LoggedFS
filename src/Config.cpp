/*****************************************************************************
 * Original Author:   Rémi Flament <remipouak@yahoo.fr>
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

#include "Format.h"
#include "FSOperations.h"
#include "Globals.h"

Config::Config()
{
    // default values
    enabled=true; 
    pNameEnabled=true;
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
							}
						}
					else if (xmlStrcmp(attr->name,PNAME_ENABLED)==0)
						{
						//enable or disable process name prints in loggedfs
						if (xmlStrcmp(attr->children->content,xmlCharStrdup("true"))!=0)
							{
							pNameEnabled=false;
							}
						}
					else if (xmlStrcmp(attr->name,FORMAT)==0)
						{
							//default format loggedfs
							this->defaultformat = Format((const char*)attr->children->content);
						}
					else
						LOG_ERROR(Globals::instance()->errlog) << "unknown attribute " << attr->name << std::endl;

				    attr=attr->next;
				}
			}
		if (xmlStrcmp(cur_node->name,INCLUDE)==0 || xmlStrcmp(cur_node->name,EXCLUDE)==0)
			{
			Filter* filter=new Filter();
			filter->setFormat(this->defaultformat);
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
					else
						LOG_ERROR(Globals::instance()->errlog) << "unknown attribute " << attr->name << std::endl;

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

bool Config::fuzzyShouldLog(int action)
{
	// this method tries to surley answer the question: shall 'action' not be logged?
	// thus the logic in this function is kind of non intuitiv

    if (!enabled) return false;

	static std::bitset<64> shallnotlog; // if a method is enabled here, we can be sure that it shall not be logged
    if(shallnotlog[action]) return false;
	if (includes.size()>0 && excludes.size()<=0)
	{
		shallnotlog[action] = true;

		for (unsigned int i=0;i<includes.size();i++)
		{
			Filter f=includes[i];
			if (f.matches(FSOperations::actions[action],f.getAction())){
				shallnotlog[action] = false;
				return false;
			}
		}
	}

	return true;
}
bool Config::shouldLog(const char* filename, int uid, const char* action, const char* retname, Format **format)
{
    bool should=false;

    if (enabled)
    {
    	if (includes.size()>0)
		{
			for (unsigned int i=0;i<includes.size();i++)
			{
				Filter f=includes[i];
				if (f.matches(filename,uid,action,retname)){
					*format = &(f.getFormat());
					should=true;
					break;
				}
			}
			for (unsigned int i=0;should && i<excludes.size();i++)
			{
				Filter f=excludes[i];
				if (f.matches(filename,uid,action,retname)){
					should=false;
					break;
				}
			}
		}
    	else if(excludes.size()>0)
    	{
    		should = true;
			for (unsigned int i=0;should && i<excludes.size();i++)
			{
				Filter f=excludes[i];
				if (f.matches(filename,uid,action,retname)){
					should=false;
					break;
				}
			}
    	}
		else
		{
			*format = &this->defaultformat;
			should=true;
		}
    }

    return should;
}
