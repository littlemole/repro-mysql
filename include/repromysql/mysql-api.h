#ifndef _MOL_DEF_GUARD_DEFINE_MOD_HTTP_REQUEST_MYSQL_DEF_GUARD_
#define _MOL_DEF_GUARD_DEFINE_MOD_HTTP_REQUEST_MYSQL_DEF_GUARD_

#include "repromysql/mysql-bindings.h"
  
//////////////////////////////////////////////////////////////
 
namespace repromysql {

class mysql;
class result;
class statement;


class result : public std::enable_shared_from_this<result>
{
public:
	typedef std::shared_ptr<result> Ptr;

	result(std::shared_ptr<statement> st);

	~result() {}

	bool fetch();

	const int fields() const;
	const Retval& field(int i) const;

	std::shared_ptr<statement> st();
	std::shared_ptr<mysql> con();

private:

	int column_count_;
	std::vector<std::shared_ptr<Retval>> fields_;
	std::shared_ptr<MYSQL_BIND> bind_;
	std::shared_ptr<statement> st_;
};


class statement : public std::enable_shared_from_this<statement>
{
friend class result;
public:
	typedef std::shared_ptr<statement> Ptr;

	statement(std::shared_ptr<mysql> con,MYSQL_STMT* st);
	~statement() {}

	MYSQL_FIELD* field( int i ) const;
	int param_count() const;
	int column_count() const;

	template<class T>
	void bind(int index, T value, enum_field_types t)
	{
		int i = index-1;
		if ( i < 0 || i >= params_.size())
		{
			throw repro::Ex("invalid param index");
		}
		params_[i]->set(value,t);
	}


	template<class T>
	void bind(int index, T value)
	{
		int i = index-1;
		if ( i < 0 || i >= params_.size())
		{
			throw repro::Ex("invalid param index");
		}
		params_[i]->set(value);
	}

	Ptr execute();
	result::Ptr query();

	std::shared_ptr<mysql> con();

	MYSQL_STMT* st();

private:

	std::vector<MYSQL_BIND>	bind_;
	std::vector<std::shared_ptr<Param>> params_;
	std::shared_ptr<mysql> mysql_;
	std::shared_ptr<MYSQL_STMT> stmt_;
	std::shared_ptr<MYSQL_RES> prepare_meta_result_;
	int param_count_;
	int column_count_;
};



class mysql : public std::enable_shared_from_this<mysql>
{
public:
	typedef std::shared_ptr<mysql> Ptr;

	mysql();
	~mysql();

	Ptr static connect( const std::string& host, const std::string& user, const std::string& pwd, const std::string& db);

	Ptr execute( std::string sql);
	ResultSet query(std::string sql);
	statement::Ptr prepare(std::string sql);



	my_ulonglong insert_id();

	MYSQL* con();
	void close();

	Ptr tx_start()
	{
		execute("START TRANSACTION");
		return shared_from_this();
	}

	Ptr commit()
	{
		execute("COMMIT");
		return shared_from_this();
	}


	Ptr rollback()
	{
		execute("ROLLBACK");
		return shared_from_this();
	}

	std::string quote(const std::string& s)
	{
		std::vector<char> buf(s.size()*2+1);
		unsigned long len =  mysql_real_escape_string(con_, &(buf[0]), s.c_str(), s.size());
		return std::string( &(buf[0]),len);
	}

private:
	MYSQL* con_;
	my_ulonglong id_;
};



class Result
{
public:
	Result(result::Ptr r)
	: res_(r)
	{}

	const Retval& operator[] ( int index )
	{
		return res_->field(index);
	}

	result* operator->()
	{
		return res_.operator ->();
	}

private:
	result::Ptr res_;
};


class MySQL 
{
public:
	MySQL();	
	MySQL(const MySQL& rhs) {};	
	MySQL(MySQL&& rhs) {};	
};

//////////////////////////////////////////////////////////////

}

#endif

