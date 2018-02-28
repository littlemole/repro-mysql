#define _XOPEN_SOURCE       /* See feature_test_macros(7) */

#include "gtest/gtest.h"
#include <memory>
#include <list>
#include <utility>
#include <iostream>
#include <string>
#include <exception>
#include <functional>
#include "reprocpp/after.h"
#include "priocpp/loop.h"
#include "priocpp/api.h"
#include "priocpp/task.h"
#include "repromysql/mysql-api.h"
#include "repromysql/mysql-async.h"
#include "test.h"
#include <stdio.h>
 
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <crypt.h>

using namespace prio;
using namespace repromysql;



class BasicTest : public ::testing::Test {
 protected:

  static void SetUpTestCase() {

		auto m = repromysql::mysql::connect("localhost","test", "test", "test");

		m->execute("DROP TABLE  if exists test");
		m->execute("CREATE TABLE test (id int auto_increment primary key, item varchar(64), value varchar(1024))");
		m->execute("INSERT INTO test (item, value) VALUES('a','a value')");
		m->execute("INSERT INTO test (item, value) VALUES('b','b value')");
  }

  virtual void SetUp() {
	 // MOL_TEST_PRINT_CNTS();
  }

  virtual void TearDown() {
	 // MOL_TEST_PRINT_CNTS();
  }



}; // end test setup




TEST_F(BasicTest, SimpleSql)
{

	auto m = repromysql::mysql::connect("localhost","test", "test", "test");

	ResultSet r = m->query("show tables;");

	std::string result;
	while(r.fetch())
	{
		result = r[0];
	}
	EXPECT_STREQ("test",result.c_str());
	MOL_TEST_ASSERT_CNTS(0,0);
}



TEST_F(BasicTest, SimpleSqlStatement)
{

	auto m = repromysql::mysql::connect("localhost","test", "test", "test");

	auto ps = m->prepare("select count(id) from test");

	auto r = ps->query();

	int i = 0;
	while(r->fetch())
	{
		i = r->field(0).getInt();
	}

	EXPECT_EQ(2,i);
	MOL_TEST_ASSERT_CNTS(0,0);
}

TEST_F(BasicTest, SimpleAsyncSqlStatement)
{

	signal(SIGINT).then([](int s){ std::cout << "SIGINT" << std::endl; theLoop().exit(); });

	MysqlPool pool("mysql://test:test@localhost/test");

	std::string result;

	pool.con()
	.then( [](mysql_async::Ptr m)
	{
		return m->prepare("SELECT value from test where id = ?");
	})
	.then( [](statement_async::Ptr stm)
	{
		stm->bind(1,"1");
		return stm->query();
	})
	.then( [&result](result_async::Ptr r)
	{
		while(r->fetch())
		{
			result = r->field(0).getString();
		}
		theLoop().exit();
	})
	.otherwise( [](const std::exception& ex)
	{
		std::cout << ex.what() << std::endl;
		theLoop().exit();
	});

	theLoop().run();

	EXPECT_STREQ("a value",result.c_str());
	MOL_TEST_ASSERT_CNTS(0,0);
}


TEST_F(BasicTest, SimpleCombinedAsyncSqlStatement)
{

	signal(SIGINT).then([](int s){ std::cout << "SIGINT" << std::endl; theLoop().exit(); });

	MysqlPool pool("mysql://test:test@localhost/test");

	std::string result;

	pool.query("SELECT value from test where id = ?", "1")
	.then( [&result](result_async::Ptr r)
	{
		while(r->fetch())
		{
			result = r->field(0).getString();
		}
		theLoop().exit();
	})
	.otherwise( [](const std::exception& ex)
	{
		std::cout << ex.what() << std::endl;
		theLoop().exit();
	});

	theLoop().run();

	EXPECT_STREQ("a value",result.c_str());
	MOL_TEST_ASSERT_CNTS(0,0);
}


TEST_F(BasicTest, TxCombinedAsyncSqlStatement)
{

	signal(SIGINT).then([](int s){ std::cout << "SIGINT" << std::endl; theLoop().exit(); });

	MysqlPool pool("mysql://test:test@localhost/test");

	int cnt = 0;

	pool.execute("DELETE from test where id > 2")
	.then( [](mysql_async::Ptr m)
	{
		return m->tx_start();
	})
	.then( [](mysql_async::Ptr m)
	{
		return m->execute("INSERT INTO test (item,value) VALUES(?,?)", "test", "test");
	})
	.then( [](mysql_async::Ptr m)
	{
		return m->commit();
	})
	.then( [](mysql_async::Ptr m)
	{
		return m->query("SELECT count(id) from test where id > ?",2);
	})
	.then( [&cnt](result_async::Ptr r)
	{
		while(r->fetch())
		{
			cnt = r->field(0).getInt();
		}
		theLoop().exit();
	})
	.otherwise( [](const std::exception& ex)
	{
		std::cout << ex.what() << std::endl;
		theLoop().exit();
	});

	theLoop().run();

	EXPECT_EQ(1,cnt);
	MOL_TEST_ASSERT_CNTS(0,0);
}



TEST_F(BasicTest, TxCombinedAsyncSqlStatementRollback)
{

	signal(SIGINT).then([](int s){ std::cout << "SIGINT" << std::endl; theLoop().exit(); });

	MysqlPool pool("mysql://test:test@localhost/test");


	int cnt = 0;

	pool.execute("DELETE from test where id > 2")
	.then( [](mysql_async::Ptr m)
	{
		return m->tx_start();
	})
	.then( [](mysql_async::Ptr m)
	{
		return m->execute("INSERT INTO test (item,value) VALUES(?,?)", "test", "test");
	})
	.then( [](mysql_async::Ptr m)
	{
		return m->rollback();
	})
	.then( [](mysql_async::Ptr m)
	{
		return m->query("SELECT Count(id) from test where id > ? ", 1);
	})
	.then( [&cnt](result_async::Ptr r)
	{
		while(r->fetch())
		{
			cnt = r->field(0).getInt();
		}
		theLoop().exit();
	})
	.otherwise( [](const std::exception& ex)
	{
		std::cout << ex.what() << std::endl;
		theLoop().exit();
	});

	theLoop().run();

	EXPECT_EQ(1,cnt);
	MOL_TEST_ASSERT_CNTS(0,0);
}

TEST_F(BasicTest, SimpleCombinedDoubleAsyncSqlStatement)
{

	signal(SIGINT).then([](int s){ std::cout << "SIGINT" << std::endl; theLoop().exit(); });

	MysqlPool pool("mysql://test:test@localhost/test");

	std::string result;

	pool.query("SELECT value from test where id = ?", "1")
	.then( [&result](result_async::Ptr r)
	{
		while(r->fetch())
		{
			result.append(r->field(0).getString());
		}

		return r->con()->query("SELECT value from test where id = ?", 2);
	})
	.then( [&result](result_async::Ptr r)
	{
		while(r->fetch())
		{
			result.append(r->field(0).getString());
		}

		theLoop().exit();
	})
	.otherwise( [](const std::exception& ex)
	{
		std::cout << ex.what() << std::endl;
		theLoop().exit();
	});

	theLoop().run();

	EXPECT_STREQ("a valueb value",result.c_str());
	MOL_TEST_ASSERT_CNTS(0,0);
}

int main(int argc, char **argv) {

	prio::init();
	mysql_library_init(0,0,0);
	mysql_thread_init();
    ::testing::InitGoogleTest(&argc, argv);

    theLoop().onThreadStart([]()
    {
    	mysql_thread_init();
    });
    theLoop().onThreadShutdown([]()
    {
    	mysql_thread_end();
    });

    int r = RUN_ALL_TESTS();

    return r;
}