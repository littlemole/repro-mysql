#include "repromysql/mysql-api.h"
#include <iomanip>

namespace repromysql {

mysql::mysql()
	: con_(nullptr), id_(0)
{
	con_ = mysql_init(NULL);
	if(con_ == NULL) {
		throw repro::Ex("mysql init failed");
	}
}




mysql::Ptr mysql::connect( const std::string& host, const std::string& user, const std::string& pwd, const std::string& db)
{
	Ptr ptr = std::make_shared<mysql>();

	if (mysql_real_connect(ptr->con(), host.c_str(), user.c_str(), pwd.c_str(),
			db.c_str(), 0, NULL, 0) == NULL)
	{
		throw repro::Ex("mysql connect failed");
	}
	return ptr;
}

mysql::Ptr mysql::execute( std::string sql) {

	Ptr ptr = shared_from_this();

	if (mysql_query(ptr->con(), sql.c_str()))
	{
		repro::Ex ex("mysql query failed");
		throw ex;
	}

	ptr->id_ = mysql_insert_id(ptr->con());
	return ptr;
}

my_ulonglong mysql::insert_id() {

	return id_;
}

ResultSet mysql::query(std::string sql)
{
	Ptr ptr = shared_from_this();

	if (mysql_query(ptr->con(), sql.c_str()))
	{
		repro::Ex ex("mysql query failed");
		throw ex;
	}

	MYSQL_RES* result = mysql_store_result(ptr->con());

	return ResultSet(result);
}


mysql::~mysql()
{
	close();
}

MYSQL* mysql::con()
{
	return con_;
}

void mysql::close()
{
	if(con_)
	{
		mysql_close(con_);
	}
	con_ = 0;
}


statement::Ptr mysql::prepare(std::string sql)
{
	auto ptr = shared_from_this();

	MYSQL_STMT* stmt = mysql_stmt_init(ptr->con());

	int mb =  1;
	mysql_stmt_attr_set( stmt, STMT_ATTR_UPDATE_MAX_LENGTH, &mb);

	if ( mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) )
		throw repro::Ex("mysql_stmt_prepare failed!");

	return std::make_shared<statement>(ptr,stmt);
}

/////////////////////////////////////////////////////////////////////////////////////////////




statement::statement(std::shared_ptr<mysql> con,MYSQL_STMT* st)
	: mysql_(con),
	  stmt_(st,[](MYSQL_STMT* s){ mysql_stmt_close(s); }),
	  param_count_(0),
	  column_count_(0)
{
	param_count_ = mysql_stmt_param_count(st);

	MYSQL_RES* rm = mysql_stmt_result_metadata(st);
	if ( rm )
	{
		prepare_meta_result_.reset(rm,[](MYSQL_RES* r){mysql_free_result(r);});

		column_count_ = mysql_num_fields(rm);
	}

	if ( param_count_ > 0 )
	{
		for ( int i = 0; i < param_count_; i++)
		{
			bind_.push_back(MYSQL_BIND());
			memset(&(bind_[i]),0,sizeof(MYSQL_BIND));

			params_.push_back(std::make_shared<Param>());
		}
	}
}


MYSQL_FIELD* statement::field( int i ) const
{
	if(!prepare_meta_result_)
		throw repro::Ex("no result yet");

	if ( i < 0 || i >= column_count_ )
		throw repro::Ex("invalid field!");

	return mysql_fetch_field_direct(prepare_meta_result_.get(), i);
}

int statement::param_count() const {
	return param_count_;
}

int statement::column_count() const {
	return column_count_;
}


MYSQL_STMT* statement::st()
{
	return stmt_.get();
}

std::shared_ptr<mysql> statement::con()
{
	return mysql_;
}


result::Ptr statement::query()
{
	auto ptr = shared_from_this();

	mysql_stmt_free_result(ptr->stmt_.get());

	if ( ptr->param_count_ > 0 )
	{
		for ( int i = 0; i < ptr->param_count_; i++)
		{
			ptr->params_[i]->bind(ptr->bind_[i]);
		}

		if (mysql_stmt_bind_param(ptr->stmt_.get(), &(ptr->bind_[0])) )
			throw repro::Ex("mysql_stmt_bind_param failed!");
	}

	if (mysql_stmt_execute(ptr->stmt_.get()))
	{
		std::ostringstream oss;
		oss << "mysql_stmt_execute failed: " << mysql_stmt_error(ptr->stmt_.get());
		throw repro::Ex(oss.str());
	};

	return std::make_shared<result>(ptr);
}

statement::Ptr statement::execute()
{
	auto ptr = shared_from_this();

	mysql_stmt_free_result(ptr->stmt_.get());

	if ( ptr->param_count_ > 0 )
	{
		for ( int i = 0; i < ptr->param_count_; i++)
		{
			ptr->params_[i]->bind(ptr->bind_[i]);
		}

		if (mysql_stmt_bind_param(ptr->stmt_.get(), &(ptr->bind_[0])) )
			throw repro::Ex("mysql_stmt_bind_param failed!");
	}

	if (mysql_stmt_execute(ptr->stmt_.get()))
	{
		std::ostringstream oss;
		oss << "mysql_stmt_execute failed: " << mysql_stmt_error(ptr->stmt_.get());
		throw repro::Ex(oss.str());
	};

	return ptr;
}






std::shared_ptr<statement> result::st()
{
	return st_;
}

std::shared_ptr<mysql> result::con()
{
	return st_->con();
}

result::result(std::shared_ptr<statement> st)
  : column_count_(st->column_count()),st_(st)
{

	if ( column_count_ > 0 )
	{
		bind_.reset( new MYSQL_BIND[column_count_],[](MYSQL_BIND* m){ delete[] m;} );
	}

	fields_.clear();
	for( int i = 0; i < column_count_; i++)
	{
		memset(&(bind_.get()[i]),0,sizeof(MYSQL_BIND));

		MYSQL_FIELD* field = st->field(i);
		fields_.push_back( std::make_shared<Retval>( field->name, field->type, field->length) );
		fields_[i].get()->bind(bind_.get()[i]);
	}
	if (mysql_stmt_store_result(st->st()))
		throw repro::Ex("mysql_stmt_store_results failed!");

	if(column_count_ > 0)
	{
		if (mysql_stmt_bind_result(st.get()->stmt_.get(), bind_.get()))
			throw repro::Ex("mysql_stmt_bind_result failed!");
	}

}

const int result::fields() const
{
	return column_count_;
}

const Retval& result::field(int i) const
{
	return *(fields_[i].get());
}

bool result::fetch()
{
	if ( mysql_stmt_fetch(st_.get()->stmt_.get()) )
	{
		return false;
	}
	return true;
}


///////////////////////////////////////////////////////////////


}

///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////


