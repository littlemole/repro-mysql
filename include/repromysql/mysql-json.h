#ifndef _MOL_DEF_GUARD_DEFINE_MOD_HTTP_REQUEST_MYSQL_JSON_DEF_GUARD_
#define _MOL_DEF_GUARD_DEFINE_MOD_HTTP_REQUEST_MYSQL_JSON_DEF_GUARD_

#include "repromysql/mysql-async.h"
#include <json/json.h>
//////////////////////////////////////////////////////////////
 

 
namespace repromysql {



inline Json::Value toJson(result::Ptr r)
{
	Json::Value result(Json::arrayValue);

	while(r->fetch())
	{
		Json::Value obj(Json::objectValue);

		int n = r->fields();
		for ( int i = 0; i < n; i++)
		{
			enum_field_types type = r->field(i).type();
			switch( type )
			{
				case MYSQL_TYPE_TINY:
				case MYSQL_TYPE_SHORT:
				case MYSQL_TYPE_LONG:
				{ 
					obj[r->field(i).name()] = r->field(i).getInt();
					break;
				}
				case MYSQL_TYPE_FLOAT:
				case MYSQL_TYPE_DOUBLE:
				{
					obj[r->field(i).name()] = r->field(i).getDouble();
					break;
				}
				case MYSQL_TYPE_LONGLONG:
				case MYSQL_TYPE_INT24:
				{
					obj[r->field(i).name()] = Json::LargestUInt(r->field(i).getLongLong());
					break;
				}
				case MYSQL_TYPE_TIME:
				case MYSQL_TYPE_DATE:
				case MYSQL_TYPE_DATETIME:
				case MYSQL_TYPE_TIMESTAMP:
				case MYSQL_TYPE_VAR_STRING:
				case MYSQL_TYPE_STRING:
				case MYSQL_TYPE_BLOB:
				case MYSQL_TYPE_DECIMAL:
				case MYSQL_TYPE_NEWDECIMAL:
				{
					obj[r->field(i).name()] = r->field(i).getString();
					break;
				}
				case MYSQL_TYPE_NULL:
				{
					obj[r->field(i).name()] = Json::Value(Json::nullValue);
					break;
				}
				default :
				{
					std::ostringstream oss;
					oss << "unsupported mysql data type: " << type;
					throw repro::Ex(oss.str());
					break;
				}

			}
		}

		result.append(obj);
	}

	return result;
}

//////////////////////////////////////////////////////////////

inline Json::Value toJson(result_async::Ptr r)
{
	Json::Value result(Json::arrayValue);

	while(r->fetch())
	{
		Json::Value obj(Json::objectValue);

		int n = r->fields();
		for ( int i = 0; i < n; i++)
		{
			if(r->field(i).null()) 
			{
				obj[r->field(i).name()] = Json::Value(Json::nullValue);
				continue;
			}
					
			enum_field_types type = r->field(i).type();
			switch( type )
			{
				case MYSQL_TYPE_TINY:
				case MYSQL_TYPE_SHORT:
				case MYSQL_TYPE_LONG:
				{ 
					obj[r->field(i).name()] = r->field(i).getInt();
					break;
				}
				case MYSQL_TYPE_FLOAT:
				case MYSQL_TYPE_DOUBLE:
				{
					obj[r->field(i).name()] = r->field(i).getDouble();
					break;
				}
				case MYSQL_TYPE_LONGLONG:
				case MYSQL_TYPE_INT24:
				{
					obj[r->field(i).name()] = Json::LargestUInt(r->field(i).getLongLong());
					break;
				}
				case MYSQL_TYPE_TIME:
				case MYSQL_TYPE_DATE:
				case MYSQL_TYPE_DATETIME:
				case MYSQL_TYPE_TIMESTAMP:
				case MYSQL_TYPE_VAR_STRING:
				case MYSQL_TYPE_STRING:
				case MYSQL_TYPE_BLOB:
				case MYSQL_TYPE_DECIMAL:
				case MYSQL_TYPE_NEWDECIMAL:
				{
					obj[r->field(i).name()] = r->field(i).getString();
					break;
				}
				case MYSQL_TYPE_NULL:
				{
					obj[r->field(i).name()] = Json::Value(Json::nullValue);
					break;
				}
				default :
				{
					std::ostringstream oss;
					oss << "unsupported mysql data type: " << type;
					throw repro::Ex(oss.str());
					break;
				}

			}
		}

		result.append(obj);
	}

	return result;
}


//////////////////////////////////////////////////////////////

}

#endif

