#include "repromysql/mysql-bindings.h"
#include <iomanip>
#include <regex>

namespace repromysql {

///////////////////////////////////////////////////////////////

Binding::Binding()
{
	init();
}

Binding::~Binding()
{
}

void Binding::init()
{
	is_null_        = false;
	is_err_         = false;

	buf_.reset();
	maxlen_  	    = 0;
	u_.longlongval_ = 0;

}

void Binding::bind(MYSQL_BIND& bind )
{
	is_null_        = false;
	is_err_         = false;

	bind.buffer_type = (enum_field_types) type_;
	switch( type_ )
	{
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_LONG:
		{
			bind.buffer = (char*)&u_.intval_;
			break;
		}
		case MYSQL_TYPE_LONGLONG:
		case MYSQL_TYPE_INT24:
		{
			bind.buffer = (char*)&u_.longlongval_;
			break;
		}
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
		{
			bind.buffer = (char*)&u_.doubleval_;
			break;
		}
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP:
		{
			bind.buffer = (char*)&u_.timeval_;
			break;
		}
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
		{
			bind.buffer 		= buf_.get();
			bind.buffer_length = maxlen_;
			bind.length  		= &u_.strlen_;
			break;
		}
		default :
		{
			std::ostringstream oss;
			oss << "bind: unsupported mysql data type: " << type_;
			throw repro::Ex(oss.str());
			break;
		}

	}
	bind.is_null = &is_null_;
	bind.error   = &is_err_;
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

Param::Param( )
{
	REPRO_MONITOR_INCR(mysqlParam);	
}

Param::Param( enum_field_types type, int size = 0 )
{
	type_ = type;
	maxlen_ = size;
	REPRO_MONITOR_INCR(mysqlParam);	
}

void Param::set( const char* s, enum_field_types type )
{
	std::string tmp(s);
	return set(tmp,type);
}

int asInt(const std::string& s)
{
	std::istringstream iss(s);
	int i;
	iss >> i;
	return i;
}

void Param::set( const std::string& s, enum_field_types type )
{
	type_ = type;
	std::istringstream iss(s);
	switch(type_)
	{
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_LONG:
		{
			int i;
			iss >> i;
			u_.intval_ = i;
			break;
		}
		case MYSQL_TYPE_LONGLONG:
		case MYSQL_TYPE_INT24:
		{
			long long l;
			iss >> l;
			u_.longlongval_ = l;
			break;
		}
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
		{
			double d;
			iss >> d;
			u_.doubleval_ = d;
			break;
		}
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP:
		{
			memset(&u_.timeval_,0,sizeof(MYSQL_TIME));

			std::cmatch res;
			std::regex rx("^(\\d\\d\\d\\d)-(\\d\\d)-(\\d\\d) ?(\\d\\d)?:?(\\d\\d)?:?(\\d\\d)?");

			std::regex_search(s.c_str(),res,rx);
			int nmatches = res.size();

			if ( nmatches > 1) {
				u_.timeval_.year = asInt(res.str(1));
			}
			if ( nmatches > 2) {
				u_.timeval_.month = asInt(res.str(2));
			}
			if ( nmatches > 3) {
				u_.timeval_.day = asInt(res.str(3));
			}
			if ( nmatches > 4) {
				u_.timeval_.hour = asInt(res.str(4));
			}
			if ( nmatches > 5) {
				u_.timeval_.minute = asInt(res.str(5));
			}
			if ( nmatches > 6) {
				u_.timeval_.second = asInt(res.str(6));
			}
			std::ostringstream oss;
			oss << u_.timeval_.year << "-" << u_.timeval_.month << "-" << u_.timeval_.day << " ";
			oss << u_.timeval_.hour << ":" << u_.timeval_.minute << ":" << u_.timeval_.second;
			std::cerr << oss.str() << std::endl;
			break;
		}
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
		{
			int n = s.size()+1;
			char* buf = new char[n];
			strncpy(buf,s.c_str(),n);
			buf_.reset(buf,[](const char* c){delete[] c;});
			u_.strlen_ = s.size();
			break;
		}
		default :
		{
			std::ostringstream oss;
			oss << "(Set) unsupported mysql data type: " << type_;
			throw repro::Ex(oss.str());
			break;
		}

	}
}

void Param::set( MYSQL_TIME& ts, enum_field_types type )
{
	type_ = type;

	switch(type_)
	{
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_LONG:
		case MYSQL_TYPE_LONGLONG:
		case MYSQL_TYPE_INT24:
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
		{
			break;
		}
		case MYSQL_TYPE_TIME:
		case MYSQL_TYPE_DATE:
		case MYSQL_TYPE_DATETIME:
		case MYSQL_TYPE_TIMESTAMP:
		{
			u_.timeval_ = ts;
			break;
		}
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
		{
			break;
		}
		default :
		{
			std::ostringstream oss;
			oss << "(time) unsupported mysql data type: " << type_;
			throw repro::Ex(oss.str());
			break;
		}

	}
}


void Param::setNull()
{
	is_null_ = true;
}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////
// output bind buffers for prepared statements
///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

Retval::Retval(const char* name, enum_field_types type, int size )
	: name_(name)
{
	type_ = type;

	switch( type )
	{
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
		{
			maxlen_ = u_.strlen_ = size+1;
			buf_.reset(new char[maxlen_],[](const char* c){delete[] c;});
			break;
		}
		default :
		{

		}
	}
	REPRO_MONITOR_INCR(mysqlRetval);
}



const std::string Retval::getString() const
{
	switch( type_ )
	{
		case MYSQL_TYPE_TINY:
		case MYSQL_TYPE_SHORT:
		case MYSQL_TYPE_LONG:
		{
			std::ostringstream oss;
			oss << u_.intval_;
			return oss.str();
		}
		case MYSQL_TYPE_FLOAT:
		case MYSQL_TYPE_DOUBLE:
		{
			std::ostringstream oss;
			oss << u_.doubleval_;
			return oss.str();
		}
		case MYSQL_TYPE_LONGLONG:
		case MYSQL_TYPE_INT24:
		{
			std::ostringstream oss;
			oss << u_.longlongval_;
			return oss.str();
		}
		case MYSQL_TYPE_TIME:
		{
			std::ostringstream oss;
			oss << u_.timeval_.hour << ":" << u_.timeval_.minute << ":" << u_.timeval_.second;
			return oss.str();
			break;
		}
		case MYSQL_TYPE_DATE:
		{
			std::ostringstream oss;
			oss << u_.timeval_.year << "-" << u_.timeval_.month << "-" << u_.timeval_.day;
			return oss.str();
			break;
		}
		case MYSQL_TYPE_DATETIME:
		{
			std::ostringstream oss;
			oss << u_.timeval_.year << "-" << u_.timeval_.month << "-" << u_.timeval_.day << " ";
			oss << u_.timeval_.hour << ":" << u_.timeval_.minute;
			return oss.str();
			break;
		}
		case MYSQL_TYPE_TIMESTAMP:
		{
			std::ostringstream oss;
			oss << u_.timeval_.year << "-" << u_.timeval_.month << "-" << u_.timeval_.day << " ";
			oss << u_.timeval_.hour << ":" << u_.timeval_.minute << ":" << u_.timeval_.second;
			return oss.str();
		}
		case MYSQL_TYPE_VAR_STRING:
		case MYSQL_TYPE_STRING:
		case MYSQL_TYPE_BLOB:
		case MYSQL_TYPE_DECIMAL:
		case MYSQL_TYPE_NEWDECIMAL:
		{
			if (!buf_)
				return "";
			if (is_null_)
				return "";
			if (is_err_)
				return "";

			return buf_.get();
		}
		case MYSQL_TYPE_NULL:
		{
			return "";
		}
		default :
		{
			std::ostringstream oss;
			oss << "unsupported mysql data type: " << type_;
			throw repro::Ex(oss.str());
			break;
		}

	}
	return "";
}



bool Retval::operator()()
{
	return (!err()) && (!null());
}

const int Retval::getInt() const
{
	return getNumber<int>();
}

const float Retval::getFloat() const
{
	return getNumber<float>();
}

const double Retval::getDouble() const
{
	return getNumber<double>();
}

const MYSQL_TIME& Retval::getTime() const
{
	return u_.timeval_;
}

const long long int Retval::getLongLong() const
{
	return getNumber<long long>();
}



ResultSet::ResultSet(MYSQL_RES* res)
	: result_(res, [](MYSQL_RES* r){mysql_free_result(r);} ), num_fields_(0),row_(nullptr)
{
	if(result_ != NULL) {
		num_fields_ = mysql_num_fields(result_.get());
	}

	REPRO_MONITOR_INCR(mysqlResultSet);

}

char* ResultSet::operator[](size_t i)
{
	return row_[i];
}


bool ResultSet::fetch()
{
	if (result_ == NULL)
	{
		return false;
	}

	row_ = mysql_fetch_row(result_.get());

	return row_ != 0;
}

int ResultSet::num_fields() {
	return num_fields_;
}



}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////


