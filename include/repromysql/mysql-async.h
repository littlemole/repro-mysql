#ifndef _MOL_DEF_GUARD_DEFINE_MOD_HTTP_REQUEST_MYSQL_ASYNC_DEF_GUARD_
#define _MOL_DEF_GUARD_DEFINE_MOD_HTTP_REQUEST_MYSQL_ASYNC_DEF_GUARD_

#include "repromysql/mysql-api.h"
#include "priocpp/ResourcePool.h"

//////////////////////////////////////////////////////////////

namespace repromysql {

class mysql_async;
class result_async;
class statement_async;


//////////////////////////////////////////////////////////////


struct MysqlLocator
{
	typedef MYSQL type;

	static repro::Future<type*> retrieve(const std::string& url);
	static void free( type* t);
};

//////////////////////////////////////////////////////////////

class MysqlPool
{
public:
	typedef prio::ResourcePool<MysqlLocator> Pool;
	typedef Pool::ResourcePtr ResourcePtr;

	MysqlPool(const std::string& url, int capacity = 4)
		: url_(url), pool_(capacity)
	{}

	MysqlPool() {}

	~MysqlPool() {}

	template<class ...Args>
	repro::Future<std::shared_ptr<result_async>> query(std::string sql, Args ... args);

	template<class ...Args>
	repro::Future<std::shared_ptr<mysql_async>> execute(std::string sql, Args ... args);

	template<class F>
	repro::Future<> tx( F fun );

	repro::Future<ResourcePtr> get();
	repro::Future<ResourcePtr> operator()();

	repro::Future<std::shared_ptr<mysql_async>> con();

	void shutdown()
	{
		pool_.shutdown();
	}
private:

	std::string url_;
	Pool pool_;
};


class result_async : public std::enable_shared_from_this<result_async>
{
public:

	LITTLE_MOLE_MONITOR(MysqlAsyncResults);

	typedef std::shared_ptr<result_async> Ptr;

	result_async(std::shared_ptr<statement_async> st);

	~result_async() {}

	bool fetch();

	const int fields() const;
	const Retval& field(int i) const;
	const Retval& field(const std::string& name) const;

	std::shared_ptr<statement_async> st();
	std::shared_ptr<mysql_async> con();

private:

	int column_count_;
	std::vector<std::shared_ptr<Retval>> fields_;
	std::shared_ptr<MYSQL_BIND> bind_;

	std::shared_ptr<statement_async> st_;
};


class statement_async : public std::enable_shared_from_this<statement_async>
{
friend class result;
public:

	LITTLE_MOLE_MONITOR(MysqlAsyncStatements);

	typedef std::shared_ptr<statement_async> Ptr;

	statement_async(std::shared_ptr<mysql_async> con,MYSQL_STMT* st);
	~statement_async();

	MYSQL_FIELD* field( int i ) const;
	int param_count() const;
	int column_count() const;

	template<class T>
	void bind(int index, T value, enum_field_types t)
	{
		int i = index-1;
		if ( i < 0 || i >= (int)params_.size())
		{
			throw repro::Ex("invalid param index");
		}
		params_[i]->set(value,t);
	}


	template<class T>
	void bind(int index, T value)
	{
		int i = index-1;
		if ( i < 0 || i >= (int)params_.size())
		{
			throw repro::Ex("invalid param index");
		}
		params_[i]->set(value);
	}


	repro::Future<result_async::Ptr> query();
	repro::Future<std::shared_ptr<mysql_async>> execute();

	std::shared_ptr<mysql_async> con();

	MYSQL_STMT* st();

private:

	std::vector<MYSQL_BIND>	bind_;
	std::vector<std::shared_ptr<Param>> params_;
	std::shared_ptr<mysql_async> mysql_;
	std::shared_ptr<MYSQL_STMT> stmt_;
	std::shared_ptr<MYSQL_RES> prepare_meta_result_;
	int param_count_;
	int column_count_;
};

inline void binder(statement_async::Ptr& ptr, int i)
{

}

template<class T>
void binder(statement_async::Ptr& ptr, int i, T t)
{
	ptr->bind(i,t);
}

template<class T, class ... Args>
void binder(statement_async::Ptr& ptr,int i, T t, Args ... args)
{
	ptr->bind(i,t);
	i++;
	binder(ptr,i,args...);
}


class mysql_async : public std::enable_shared_from_this<mysql_async>
{
friend class statement_async;
friend class result_async;
public:

	LITTLE_MOLE_MONITOR(MysqlAsyncs);


	typedef std::shared_ptr<mysql_async> Ptr;
	typedef repro::Future<mysql_async::Ptr> FutureType;

	mysql_async();
	mysql_async(MysqlPool::ResourcePtr m);
	~mysql_async();

//	FutureType execute( std::string sql);
//	ResultSet::FutureType query(std::string sql);

	repro::Future<statement_async::Ptr> prepare(std::string sql);

	template<class ...Args>
	repro::Future<result_async::Ptr> query(std::string sql, Args ... args)
	{
		auto p = repro::promise<result_async::Ptr>();

		prepare(sql)
		.then( [p,args...](statement_async::Ptr stm)
		{
			binder(stm,1,args...);
			return stm->query();
		})
		.then( [p](result_async::Ptr r)
		{
			p.resolve(r);
		})
		.otherwise( [p](const std::exception& ex)
		{
			p.reject(ex);
		});

		return p.future();
	}

	template<class ...Args>
	repro::Future<mysql_async::Ptr> execute(std::string sql, Args ... args)
	{
		auto p = repro::promise<mysql_async::Ptr>();

		prepare(sql)
		.then( [p,args...](statement_async::Ptr stm)
		{
			binder(stm,1,args...);
			return stm->execute();
		})
		.then( [p](mysql_async::Ptr ptr)
		{
			p.resolve(ptr);
		})
		.otherwise( [p](const std::exception& ex)
		{
			p.reject(ex);
		});

		return p.future();
	}

	my_ulonglong insert_id();

	MYSQL* con();
	void close();

	repro::Future<mysql_async::Ptr> tx_start()
	{
		Ptr ptr = shared_from_this();

		return prio::task( [ptr] () {

			if (mysql_query(ptr->con(),"START TRANSACTION"))
			{
				ptr->mysql_->markAsInvalid();
				repro::Ex ex("mysql start tx failed");
				throw ex;
			}

			return ptr;
		});
	}

	repro::Future<mysql_async::Ptr> commit()
	{
		return execute("COMMIT");
	}


	repro::Future<mysql_async::Ptr> rollback()
	{
		return execute("ROLLBACK");
	}

	std::string quote(const std::string& s)
	{
		std::vector<char> buf(s.size()*2+1);
		unsigned long len =  mysql_real_escape_string(*mysql_, &(buf[0]), s.c_str(), s.size());
		return std::string( &(buf[0]),len);
	}


private:

	MysqlPool::ResourcePtr mysql_;
};


template<class ...Args>
repro::Future<std::shared_ptr<result_async>> MysqlPool::query(std::string sql, Args ... args)
{
	auto p = repro::promise<std::shared_ptr<result_async>>();

	con()
	.then( [p,sql,args...](mysql_async::Ptr m)
	{
		auto f = m->query(sql,args...) ;
		p.resolve( f);
	})
	.otherwise( [p](const std::exception& ex)
	{
		p.reject(ex);
	});

	return p.future();
}

template<class ...Args>
repro::Future<mysql_async::Ptr> MysqlPool::execute(std::string sql, Args ... args)
{
	auto p = repro::promise<mysql_async::Ptr>();

	con()
	.then( [p,sql,args...](mysql_async::Ptr m)
	{
		auto f = m->execute(sql,args...);
		p.resolve( f );
	})
	.otherwise( [p](const std::exception& ex)
	{
		p.reject(ex);
	});

	return p.future();
}


template<class F>
repro::Future<> MysqlPool::tx(F fun)
{
	auto p = repro::promise<>();

	con()
	.then( [p,fun](mysql_async::Ptr m)
	{
		m->tx_start()
		.then( [p,fun](mysql_async::Ptr a)
		{
			return fun(a); // must be async			
		})
		.then( [p,m]()
		{
			return m->commit();		
		})
		.then( [p](mysql_async::Ptr)
		{
			prio::nextTick([p](){ p.resolve(); });

		})
		.otherwise( [p,m](const std::exception& ex)
		{
			std::exception_ptr eptr;
			const repro::Ex* rex = dynamic_cast<const repro::Ex*>(&ex);
			if ( rex)
			{
				eptr = rex->make_exception_ptr();
			}
			else
			{
				eptr = std::make_exception_ptr(ex);
			}

			m->rollback()
			.then([p,eptr](mysql_async::Ptr a)
			{
				p.reject(eptr);
			});
		});
	})
	.otherwise( [p](const std::exception& ex)
	{
		p.reject(ex);
	});

	return p.future();
}

}

#endif

