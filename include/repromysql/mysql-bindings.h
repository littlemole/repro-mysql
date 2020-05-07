#ifndef _MOL_DEF_GUARD_DEFINE_MOD_HTTP_REQUEST_MYSQL_BIND_DEF_GUARD_
#define _MOL_DEF_GUARD_DEFINE_MOD_HTTP_REQUEST_MYSQL_BIND_DEF_GUARD_

#include "priocpp/common.h"
#include "priocpp/task.h"
#include "reprocpp/promise.h"
#include <mysql/mysql.h>
  

#if LIBMYSQL_VERSION_ID > 79999
#define mybool bool
#else
#define mybool my_bool
#endif
//////////////////////////////////////////////////////////////


namespace repromysql {


///////////////////////////////////////////////////////////////


class ResultSet
{
public:

	typedef repro::Future<ResultSet> FutureType;

	ResultSet(MYSQL_RES* res);
	~ResultSet()
	{
		REPRO_MONITOR_DECR(mysqlResultSet);
	}

	bool fetch();

	char* operator[](size_t i);

	int num_fields();

private:

	ResultSet(const ResultSet&) = delete;
	ResultSet& operator=(const ResultSet&) = delete;

	 std::shared_ptr<MYSQL_RES> result_;
	 int num_fields_;
	 MYSQL_ROW row_;
};

///////////////////////////////////////////////////////////////


class Binding
{
friend class Statement;
public:

	Binding();
	~Binding();

	bool null() const
	{
		if (is_null_)
			return true;
		return false;
	}

	bool err() const
	{
		if (is_err_)
			return true;
		return false;
	}

	void bind(MYSQL_BIND& bind );

	enum_field_types type() const { return type_; }
protected:

	void init();

	Binding(const Binding& rhs)
		: type_(rhs.type_),
		  buf_(rhs.buf_),
		  maxlen_(rhs.maxlen_),
		  is_null_(rhs.is_null_),
		  is_err_(rhs.is_err_),
		  u_(rhs.u_)
	{}


	Binding( Binding&& rhs)
		: type_(std::move(rhs.type_)),
		  buf_(std::move(rhs.buf_)),
		  maxlen_(std::move(rhs.maxlen_)),
		  is_null_(std::move(rhs.is_null_)),
		  is_err_(std::move(rhs.is_err_)),
		  u_(std::move(rhs.u_))
	{
		rhs.is_null_        = false;
		rhs.is_err_         = false;

		rhs.buf_.reset();
		rhs.maxlen_  	    = 0;
		rhs.u_.longlongval_ = 0;	
	}

	enum_field_types	type_;
	std::shared_ptr<char> buf_;
	long unsigned int	maxlen_;
	mybool				is_null_;
	mybool				is_err_;

	union
	{
		int					intval_;
		double				doubleval_;
		long long int		longlongval_;
		MYSQL_TIME			timeval_;
		long unsigned int	strlen_;
	} u_;
};

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////

class Param : public Binding
{
public:

	Param();
	Param( enum_field_types type, int size );
	~Param()
	{
		REPRO_MONITOR_DECR(mysqlParam);	
	}

	void set( const std::string& s, enum_field_types type= MYSQL_TYPE_STRING);
	void set( const char* s, enum_field_types type= MYSQL_TYPE_STRING);
	void set( MYSQL_TIME& ts, enum_field_types type= MYSQL_TYPE_DATETIME);

	template<class T>
	void set( T t, enum_field_types type = MYSQL_TYPE_LONG  )
	{
		type_ = type;
		switch(type_)
		{
			case MYSQL_TYPE_TINY:
			case MYSQL_TYPE_SHORT:
			case MYSQL_TYPE_LONG:
			{
				u_.intval_ = t;
				break;
			}
			case MYSQL_TYPE_LONGLONG:
			case MYSQL_TYPE_INT24:
			{
				u_.longlongval_ = t;
				break;
			}
			case MYSQL_TYPE_FLOAT:
			case MYSQL_TYPE_DOUBLE:
			{
				u_.doubleval_ = t;
				break;
			}
			case MYSQL_TYPE_TIME:
			case MYSQL_TYPE_DATE:
			case MYSQL_TYPE_DATETIME:
			case MYSQL_TYPE_TIMESTAMP:
			{
				memset(&u_.timeval_,0,sizeof(MYSQL_TIME));
				u_.timeval_.second = t;
				break;
			}
			case MYSQL_TYPE_VAR_STRING:
			case MYSQL_TYPE_STRING:
			case MYSQL_TYPE_BLOB:
			case MYSQL_TYPE_DECIMAL:
			case MYSQL_TYPE_NEWDECIMAL:
			{
				std::ostringstream oss;
				oss << t;
				std::string s = oss.str();

				int n = s.size()+1;
				char* buf = new char[n];
				strncpy(buf,s.c_str(),n);
				buf_.reset(buf,[](const char* c){delete[] c;});
				u_.strlen_ = s.size();
				break;
			}
			default :
			{
				throw repro::Ex("(long) unsupporteed mysql data type");
				break;
			}
		}
	}


	void setNull();
};

///////////////////////////////////////////////////////////////

class Retval : public Binding
{
public:


	Retval( const char* name, enum_field_types type, int size = 256 );

	Retval(const Retval& rhs)
		: Binding(rhs), name_(rhs.name_)
	{
		REPRO_MONITOR_INCR(mysqlRetval);	
	}

	Retval(Retval&& rhs)
		: Binding(std::move(rhs)), name_(std::move(rhs.name_))
	{

	} 


	~Retval() 
	{
		REPRO_MONITOR_DECR(mysqlRetval);	
	}

	bool operator()();

	std::string name() const { return name_; }

	const std::string getString() const;
	const int getInt() const;
	const float getFloat() const;
	const double getDouble() const;
	const MYSQL_TIME& getTime() const;
	const long long int getLongLong() const;

	template<class T>
	const T getNumber() const
	{
		if(null()){
			return 0;
		}

		switch( type_ )
		{
			case MYSQL_TYPE_TINY:
			case MYSQL_TYPE_SHORT:
			case MYSQL_TYPE_LONG:
			{
				return u_.intval_;
			}
			case MYSQL_TYPE_FLOAT:
			case MYSQL_TYPE_DOUBLE:
			{
				return u_.doubleval_;
			}
			case MYSQL_TYPE_LONGLONG:
			case MYSQL_TYPE_INT24:
			{
				return u_.longlongval_;
			}
			case MYSQL_TYPE_TIME:
			case MYSQL_TYPE_DATE:
			case MYSQL_TYPE_DATETIME:
			case MYSQL_TYPE_TIMESTAMP:
			{
				return 0;
			}
			case MYSQL_TYPE_VAR_STRING:
			case MYSQL_TYPE_STRING:
			case MYSQL_TYPE_BLOB:
			case MYSQL_TYPE_DECIMAL:
			case MYSQL_TYPE_NEWDECIMAL:
			{
				if (!buf_.get())
					return 0;
				if (is_err_)
					return 0;

				std::istringstream iss( buf_.get() );
				T t;
				iss >> t;
				return t;
			}
			case MYSQL_TYPE_NULL:
			{
				return 0;
			}
			default :
			{
				throw repro::Ex("unsupporteed mysql data type");
				break;
			}

		}
		return 0;
	}
	
private:


	std::string name_;
};

//////////////////////////////////////////////////////////////


}

#endif

