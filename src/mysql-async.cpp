#include "repromysql/mysql-async.h"
#include <iomanip>

namespace repromysql {

/////////////////////////////////////////////////////////////////////////////////////////////

mysql_async::mysql_async()
	: mysql_(nullptr)
{
	REPRO_MONITOR_INCR(mysqlAsync);	
}

mysql_async::mysql_async(MysqlPool::ResourcePtr m)
	: mysql_(m)
{
	REPRO_MONITOR_INCR(mysqlAsync);	
}

my_ulonglong mysql_async::insert_id() {

	return  mysql_insert_id(con());;
}


mysql_async::~mysql_async()
{
	REPRO_MONITOR_DECR(mysqlAsync);	
}

MYSQL* mysql_async::con()
{
	return mysql_.get();
}

void mysql_async::close()
{
	mysql_.reset();
}


statement_async::Ptr mysql_async::prepare(std::string sql)
{
	MYSQL_STMT* stmt = mysql_stmt_init(con());

	int mb =  1;
	mysql_stmt_attr_set( stmt, STMT_ATTR_UPDATE_MAX_LENGTH, &mb);

	if ( mysql_stmt_prepare(stmt, sql.c_str(), sql.size()) )
	{
		prio::Resource::invalidate(mysql_);
		throw repro::Ex("mysql_stmt_prepare failed!");
	}
	auto p = std::make_shared<statement_async>(shared_from_this(),stmt);
	return p;
}


statement_async::~statement_async()
{
	REPRO_MONITOR_DECR(mysqlAsyncStatement);	
}


statement_async::statement_async(std::shared_ptr<mysql_async> con,MYSQL_STMT* st)
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

	REPRO_MONITOR_INCR(mysqlAsyncStatement);	
}


MYSQL_FIELD* statement_async::field( int i ) const
{
	if(!prepare_meta_result_)
		throw repro::Ex("no result yet");

	if ( i < 0 || i >= column_count_ )
		throw repro::Ex("invalid field!");

	return mysql_fetch_field_direct(prepare_meta_result_.get(), i);
}

int statement_async::param_count() const {
	return param_count_;
}

int statement_async::column_count() const {
	return column_count_;
}


MYSQL_STMT* statement_async::st()
{
	return stmt_.get();
}

std::shared_ptr<mysql_async> statement_async::con()
{
	return mysql_;
}


result_async::Ptr statement_async::query()
{
	mysql_stmt_free_result(stmt_.get());

	if ( param_count_ > 0 )
	{
		for ( int i = 0; i < param_count_; i++)
		{
			params_[i]->bind(bind_[i]);
		}

		if (mysql_stmt_bind_param(stmt_.get(), &(bind_[0])) )
		{
			prio::Resource::invalidate(con()->mysql_);
			throw repro::Ex("mysql_stmt_bind_param failed!");
		}
	}

	if (mysql_stmt_execute(stmt_.get()))
	{
		prio::Resource::invalidate(con()->mysql_);
		std::ostringstream oss;
		oss << "mysql_stmt_execute failed: " << mysql_stmt_error(stmt_.get());
		throw repro::Ex(oss.str());
	};

	return std::make_shared<result_async>(shared_from_this());
}

mysql_async::Ptr statement_async::execute()
{
	mysql_stmt_free_result(stmt_.get());

	if ( param_count_ > 0 )
	{
		for ( int i = 0; i < param_count_; i++)
		{
			params_[i]->bind(bind_[i]);
		}

		if (mysql_stmt_bind_param(stmt_.get(), &(bind_[0])) )
		{
			prio::Resource::invalidate(con()->mysql_);
			throw repro::Ex("mysql_stmt_bind_param failed!");
		}
	}

	if (mysql_stmt_execute(stmt_.get()))
	{
		prio::Resource::invalidate(con()->mysql_);

		std::ostringstream oss;
		oss << "mysql_stmt_execute failed: " << mysql_stmt_error(stmt_.get());
		throw repro::Ex(oss.str());
	};

	return mysql_;
}



///////////////////////////////////////////////////////////////



std::shared_ptr<statement_async> result_async::st()
{
	return st_;
}

std::shared_ptr<mysql_async> result_async::con()
{
	return st_->con();
}

result_async::result_async(std::shared_ptr<statement_async> st)
  :  column_count_(st->column_count()),st_(st)
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
	{
		prio::Resource::invalidate(con()->mysql_);
		throw repro::Ex("mysql_stmt_store_results failed!");
	}

	if(column_count_ > 0)
	{
		if (mysql_stmt_bind_result(st_->st(), bind_.get()))
		{
			prio::Resource::invalidate(con()->mysql_);
			throw repro::Ex("mysql_stmt_bind_result failed!");
		}
	}

	REPRO_MONITOR_INCR(mysqlAsyncResult);

}

const int result_async::fields() const
{
	return column_count_;
}

const Retval& result_async::field(int i) const
{
	return *(fields_[i].get());
}


const Retval& result_async::field(const std::string& name) const
{
	for ( unsigned int i = 0; i < fields_.size(); i++)
	{
		if( fields_[i].get()->name() == name ) {
			return field(i);
		}
	}
	throw repro::Ex("unknown SQL field");
}


bool result_async::fetch()
{
	if ( mysql_stmt_fetch(st_->st()) )
	{
		return false;
	}
	return true;
}

///////////////////////////////////////////////////////////////



repro::Future<MysqlLocator::type> MysqlLocator::retrieve(const std::string& u)
{
	auto p = repro::promise<type>();

	prio::task( [u]()
	{
		prio::Url url(u);

		MYSQL* con = mysql_init(NULL);
		if(con == NULL) {
			throw repro::Ex("mysql init failed");
		}

		if (mysql_real_connect(con, url.getHost().c_str(), url.getUser().c_str(), url.getPwd().c_str(),url.getPath().substr(1).c_str(),0,NULL,0) == 0 )
		{
			mysql_close(con);
			throw repro::Ex("mysql connect failed");
		}
		if (mysql_set_character_set(con, "utf8"))
		{
			throw repro::Ex("mysql connect set utf8 failed");
		}	

		return con;
	})
	.then( [p](type r)
	{
		p.resolve(r);
	})
	.otherwise(reject(p));

	return p.future();
}

void MysqlLocator::free( MysqlLocator::type t)
{
	mysql_close(t);
}



repro::Future<MysqlPool::ResourcePtr> MysqlPool::get()
{
	auto p = repro::promise<ResourcePtr>();
	pool_.get(url_)
	.then( [p](ResourcePtr r)
	{
		p.resolve(r);
	})
	.otherwise(reject(p));

	return p.future();
}

repro::Future<MysqlPool::ResourcePtr> MysqlPool::operator()()
{
	return get();
}

repro::Future<std::shared_ptr<mysql_async>> MysqlPool::con()
{
	auto p = repro::promise<mysql_async::Ptr>();

	get()
	.then( [p] (MysqlPool::ResourcePtr ptr)
	{
		p.resolve( std::make_shared<mysql_async>(ptr) );
	})
	.otherwise(reject(p));

	return p.future();
}


}


///////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////


