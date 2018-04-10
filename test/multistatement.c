/*
  Copyright (c) 2001, 2012, Oracle and/or its affiliates. All rights reserved.
                2013, 2018 MariaDB Corporation AB

  The MySQL Connector/ODBC is licensed under the terms of the GPLv2
  <http://www.gnu.org/licenses/old-licenses/gpl-2.0.html>, like most
  MySQL Connectors. There are special exceptions to the terms and
  conditions of the GPLv2 as it is applied to this software, see the
  FLOSS License Exception
  <http://www.mysql.com/about/legal/licensing/foss-exception.html>.
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published
  by the Free Software Foundation; version 2 of the License.
  
  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
  or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
  for more details.
  
  You should have received a copy of the GNU General Public License along
  with this program; if not, write to the Free Software Foundation, Inc.,
  51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "tap.h"

ODBC_TEST(test_multi_statements)
{
  SQLLEN    num_inserted;
  SQLRETURN rc;

  OK_SIMPLE_STMT(Stmt, "DROP TABLE IF EXISTS t1");
  OK_SIMPLE_STMT(Stmt, "CREATE TABLE t1 (a int)");

  OK_SIMPLE_STMT(Stmt, "INSERT INTO t1 VALUES(1);INSERT INTO t1 VALUES(2)");

  SQLRowCount(Stmt, &num_inserted);
  diag("inserted: %ld", (long)num_inserted);
  FAIL_IF(num_inserted != 1, "Expected 1 row inserted");
  
  rc= SQLMoreResults(Stmt);
  num_inserted= 0;
  rc= SQLRowCount(Stmt, &num_inserted);
  FAIL_IF(num_inserted != 1, "Expected 1 row inserted");

  rc= SQLMoreResults(Stmt);
  FAIL_IF(rc != SQL_NO_DATA, "expected no more results");

  return OK;
}

ODBC_TEST(test_multi_on_off)
{
  SQLHENV myEnv;
  SQLHDBC myDbc;
  SQLHSTMT myStmt;
  SQLRETURN rc;

  my_options= 0;
  ODBC_Connect(&myEnv, &myDbc, &myStmt);

  rc= SQLPrepare(myStmt, "DROP TABLE IF EXISTS t1; CREATE TABLE t1(a int)", SQL_NTS);
  FAIL_IF(SQL_SUCCEEDED(rc), "Error expected"); 

  ODBC_Disconnect(myEnv, myDbc, myStmt);

  my_options= 67108866;
  ODBC_Connect(&myEnv, &myDbc, &myStmt);

  rc= SQLPrepare(myStmt, "DROP TABLE IF EXISTS t1; CREATE TABLE t1(a int)", SQL_NTS);
  FAIL_IF(!SQL_SUCCEEDED(rc), "Success expected"); 

  ODBC_Disconnect(myEnv, myDbc, myStmt);
  return OK;
}


ODBC_TEST(test_params)
{
  SQLRETURN rc;
  int       i, j;

  rc= SQLExecDirect(Stmt, "DROP TABLE IF EXISTS t1; CREATE TABLE t1(a int)", SQL_NTS);
  FAIL_IF(!SQL_SUCCEEDED(rc), "unexpected error"); 

  rc= SQLExecDirect(Stmt, "DROP TABLE IF EXISTS t2; CREATE TABLE t2(a int)", SQL_NTS);
  FAIL_IF(!SQL_SUCCEEDED(rc), "unexpected error"); 

  rc= SQLPrepare(Stmt, "INSERT INTO t1 VALUES (?), (?)", SQL_NTS);
  CHECK_STMT_RC(Stmt, rc);

  CHECK_STMT_RC(Stmt, SQLBindParam(Stmt, 1, SQL_C_LONG, SQL_INTEGER, 10, 0, &i, NULL));

  rc= SQLBindParam(Stmt, 2, SQL_C_LONG, SQL_INTEGER, 10, 0, &j, NULL);
  FAIL_IF(!SQL_SUCCEEDED(rc), "unexpected error"); 

  for (i=0; i < 100; i++)
  {
    j= i + 100;
    CHECK_STMT_RC(Stmt, SQLExecute(Stmt)); 

    while (SQLMoreResults(Stmt) == SQL_SUCCESS);
  }

  return OK;
}


#define TEST_MAX_PS_COUNT 25
ODBC_TEST(t_odbc_16)
{
  SQLLEN    num_inserted;
  SQLRETURN rc;
  int       i, prev_ps_count, curr_ps_count, increment= 0, no_increment_iterations= 0;

  /* This would be valuable for the test, as driver could even crash when PS number is exhausted.
     But changing such sensible variable in production environment can lead to a problem (if
     the test crashes and default value is not restored). Should not be run done by default */
  /* set_variable(GLOBAL, "max_prepared_stmt_count", TEST_MAX_PS_COUNT - 2); */

  GET_SERVER_STATUS(prev_ps_count, GLOBAL, "Prepared_stmt_count" );

  for (i= 0; i < TEST_MAX_PS_COUNT; ++i)
  {
    OK_SIMPLE_STMT(Stmt, "DROP TABLE IF EXISTS t1");
    OK_SIMPLE_STMT(Stmt, "CREATE TABLE t1 (a int)");

    OK_SIMPLE_STMT(Stmt, "INSERT INTO t1 VALUES(1);INSERT INTO t1 VALUES(2)");

    SQLRowCount(Stmt, &num_inserted);
    FAIL_IF(num_inserted != 1, "Expected 1 row inserted");
  
    rc= SQLMoreResults(Stmt);
    num_inserted= 0;
    rc= SQLRowCount(Stmt, &num_inserted);
    FAIL_IF(num_inserted != 1, "Expected 1 row inserted");

    rc= SQLMoreResults(Stmt);
    FAIL_IF(rc != SQL_NO_DATA, "expected no more results");
    SQLFreeStmt(Stmt, SQL_CLOSE);

    GET_SERVER_STATUS(curr_ps_count, GLOBAL, "Prepared_stmt_count" );

    if (i == 0)
    {
      increment= curr_ps_count - prev_ps_count;
    }
    else
    {
      /* If only test ran on the server, then increment would be constant on each iteration */
      if (curr_ps_count - prev_ps_count != increment)
      {
        fprintf(stdout, "# This test makes sense to run only on dedicated server!\n");
        return SKIP;
      }
    }

    prev_ps_count=  curr_ps_count;

    /* Counting iterations not incrementing ps count on the server to minimize chances
       that other processes influence that */
    if (increment == 0)
    {
      if (no_increment_iterations >= 10)
      {
        return OK;
      }
      else
      {
        ++no_increment_iterations;
      }
    }
    else
    {
      no_increment_iterations= 0;
    }
  }

  diag("Server's open PS count is %d(on %d iterations)", curr_ps_count, TEST_MAX_PS_COUNT);
  return FAIL;
}
#undef TEST_MAX_PS_COUNT

/* Test that connector does not get confused by ; inside string */
ODBC_TEST(test_semicolon)
{
  SQLCHAR val[64];

  OK_SIMPLE_STMT(Stmt, "SELECT \"Semicolon ; insert .. er... inside string\"");
  CHECK_STMT_RC(Stmt, SQLFetch(Stmt));
  IS_STR(my_fetch_str(Stmt, val, 1), "Semicolon ; insert .. er... inside string", sizeof("Semicolon ; insert .. er... inside string"));
  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_CLOSE));

  return OK;
}

/* Double quote inside single quotes caused error in parsing while
   Also tests ODBC-97*/
ODBC_TEST(t_odbc74)
{
  SQLCHAR ref[][4]={"\"", "'", "*/", "/*", "end", "one\\", "two\\"}, val[8];
  unsigned int i;
  SQLHDBC     hdbc1;
  SQLHSTMT    Stmt1;

  OK_SIMPLE_STMT(Stmt, "DROP TABLE IF EXISTS odbc74; CREATE TABLE odbc74(id INT NOT NULL PRIMARY KEY AUTO_INCREMENT,\
                        val VARCHAR(64) NOT NULL)");
  OK_SIMPLE_STMT(Stmt, "INSERT INTO odbc74 (val) VALUES('\"');INSERT INTO odbc74 (val) VALUES(\"'\");\
                        /*\"*//*'*//*/**/INSERT INTO odbc74 (val) VALUES('*/');\
                        # Pound-sign comment\"'--; insert into non_existent values(1)\n\
                        # 2 lines of comments \"'--;\n\
                        INSERT INTO odbc74 (val) VALUES('/*');-- comment\"'; insert into non_existent values(1)\n\
                        INSERT INTO odbc74 (val) VALUES('end')\n\
                        # ;Unhappy comment at the end ");
  OK_SIMPLE_STMT(Stmt, "-- comment ;1 \n\
                        # comment ;2 \n\
                        INSERT INTO odbc74 (val) VALUES('one\\\\');\
                        INSERT INTO odbc74 (val) VALUES(\"two\\\\\");");
  OK_SIMPLE_STMT(Stmt, "SELECT val FROM odbc74 ORDER BY id");

  for (i= 0; i < sizeof(ref)/sizeof(ref[0]); ++i)
  {
    CHECK_STMT_RC(Stmt, SQLFetch(Stmt));
    IS_STR(my_fetch_str(Stmt, val, 1), ref[i], sizeof(ref[i]));
  }
  EXPECT_STMT(Stmt, SQLFetch(Stmt), SQL_NO_DATA);
  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_CLOSE));

  OK_SIMPLE_STMT(Stmt, "TRUNCATE TABLE odbc74");

  AllocEnvConn(&Env, &hdbc1);
  Stmt1= DoConnect(hdbc1, NULL, NULL, NULL, 0, NULL, 0, NULL, NULL);
  FAIL_IF(Stmt1 == NULL, "Could not connect and/or allocate");

  OK_SIMPLE_STMT(Stmt1, "SET @@SESSION.sql_mode='NO_BACKSLASH_ESCAPES'");

  OK_SIMPLE_STMT(Stmt1, "INSERT INTO odbc74 (val) VALUES('one\\');\
                        INSERT INTO odbc74 (val) VALUES(\"two\\\");");
  OK_SIMPLE_STMT(Stmt1, "SELECT val FROM odbc74 ORDER BY id");

  /* We only have to last rows */
  for (i= sizeof(ref)/sizeof(ref[0]) - 2; i < sizeof(ref)/sizeof(ref[0]); ++i)
  {
    CHECK_STMT_RC(Stmt1, SQLFetch(Stmt1));
    IS_STR(my_fetch_str(Stmt1, val, 1), ref[i], sizeof(ref[i]));
  }
  EXPECT_STMT(Stmt1, SQLFetch(Stmt1), SQL_NO_DATA);

  CHECK_STMT_RC(Stmt1, SQLFreeStmt(Stmt1, SQL_DROP));
  CHECK_DBC_RC(hdbc1, SQLDisconnect(hdbc1));
  CHECK_DBC_RC(hdbc1, SQLFreeConnect(hdbc1));

  OK_SIMPLE_STMT(Stmt, "DROP TABLE IF EXISTS odbc74");

  return OK;
}

ODBC_TEST(t_odbc95)
{
  EXPECT_STMT(Stmt, SQLPrepare(Stmt, "SELECT 1;INSERT INTO non_existing VALUES(2)", SQL_NTS), SQL_ERROR);
  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_DROP));
  Stmt= NULL;

  return OK;
}

ODBC_TEST(t_odbc126)
{
  SQLCHAR Query[][24]={ "CALL odbc126_1", "CALL odbc126_2", "SELECT 1, 2; SELECT 3", "SELECT 4; SELECT 5,6" };
  unsigned int i, ExpectedRows[]= {3, 3, 1, 1};
  SQLRETURN rc, Expected= SQL_SUCCESS;

  OK_SIMPLE_STMT(Stmt, "DROP TABLE IF EXISTS odbc126");
  OK_SIMPLE_STMT(Stmt, "DROP PROCEDURE IF EXISTS odbc126_1");
  OK_SIMPLE_STMT(Stmt, "DROP PROCEDURE IF EXISTS odbc126_2");

  OK_SIMPLE_STMT(Stmt, "CREATE TABLE odbc126(col1 INT, col2 INT)");
  OK_SIMPLE_STMT(Stmt, "CREATE PROCEDURE odbc126_1()\
                        BEGIN\
                          SELECT col1, col2 FROM odbc126;\
                          SELECT col1 FROM odbc126;\
                        END");
  OK_SIMPLE_STMT(Stmt, "CREATE PROCEDURE odbc126_2()\
                        BEGIN\
                          SELECT col1 FROM odbc126;\
                          SELECT col1, col2 FROM odbc126;\
                        END");

  OK_SIMPLE_STMT(Stmt, "INSERT INTO odbc126 VALUES(1, 2), (3, 4), (5, 6)");

  for (i= 0; i < sizeof(Query)/sizeof(Query[0]); ++i)
  {
    OK_SIMPLE_STMT(Stmt, Query[i]);

    do {
      is_num(my_print_non_format_result_ex(Stmt, FALSE), ExpectedRows[i]);

      rc= SQLMoreResults(Stmt);
      is_num(rc, Expected);

      Expected= SQL_NO_DATA;

    } while (rc != SQL_NO_DATA);

    CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_CLOSE));
    Expected= SQL_SUCCESS;
  }
  
  OK_SIMPLE_STMT(Stmt, "DROP TABLE odbc126");
  OK_SIMPLE_STMT(Stmt, "DROP PROCEDURE odbc126_1");
  OK_SIMPLE_STMT(Stmt, "DROP PROCEDURE odbc126_2");

  return OK;
}

ODBC_TEST(diff_column_binding)
{
  SQLCHAR Query[][100] = {"CALL diff_column_1"};
  unsigned int i, j, ExpectedRows[]= {1, 3}, bind1, bind2;
  char bindc1[64];
  __int64 bindb1;
  SQLRETURN rc, Expected= SQL_SUCCESS;
  SQLINTEGER indicator = 0;
  SQLINTEGER indicatorc = SQL_NTS;
  

  OK_SIMPLE_STMT(Stmt, "DROP TABLE IF EXISTS diff_column_binding");
  OK_SIMPLE_STMT(Stmt, "DROP PROCEDURE IF EXISTS diff_column_binding_1");

  OK_SIMPLE_STMT(Stmt, "CREATE TABLE diff_column_binding(col1 INT, col2 VARCHAR(64), col3 BIGINT unsigned)");
  OK_SIMPLE_STMT(Stmt, "CREATE PROCEDURE diff_column_binding_1()\
                        BEGIN\
                          SELECT 1017, 1370;\
                          SELECT * FROM diff_column;\
                        END");

  OK_SIMPLE_STMT(Stmt, "INSERT INTO diff_column_binding VALUES(1370, \"abcd\", 12345), (1417, \"abcdef\", 2390), (1475, \"@1475\", 0)");
  OK_SIMPLE_STMT(Stmt, "CALL diff_column_binding_1");

  // bind first result set
  SQLBindCol(Stmt, 1, SQL_C_LONG, &bind1, sizeof(int), &indicator);
  SQLBindCol(Stmt, 2, SQL_C_LONG, &bind2, sizeof(int), &indicator);
  SQLFetch(Stmt);

  SQLMoreResults(Stmt);

  // bind second result set
  SQLBindCol(Stmt, 1, SQL_C_LONG, &bind1, sizeof(int), &indicator);
  SQLBindCol(Stmt, 2, SQL_C_CHAR, bindc1, sizeof(bindc1), &indicatorc);
  SQLBindCol(Stmt, 3, SQL_C_SBIGINT, &bindb1, sizeof(bindb1), &indicator);
  SQLFetch(Stmt);
  CHECK_STMT_RC(Stmt, SQLFreeStmt(Stmt, SQL_CLOSE));

  OK_SIMPLE_STMT(Stmt, "DROP TABLE diff_column_binding");
  OK_SIMPLE_STMT(Stmt, "DROP PROCEDURE diff_column_binding_1");

  return OK;
}

MA_ODBC_TESTS my_tests[]=
{
  {test_multi_statements, "test_multi_statements"},
  {test_multi_on_off, "test_multi_on_off"},
  {test_params, "test_params"},
  {t_odbc_16, "test_odbc_16"},
  {test_semicolon, "test_semicolon_in_string"},
  {t_odbc74, "t_odbc74and_odbc97"},
  {t_odbc95, "t_odbc95"},
  {t_odbc126, "t_odbc126"},
  {diff_column_binding, "diff_column_binding"},
  {NULL, NULL}
};

int main(int argc, char **argv)
{
  int tests= sizeof(my_tests)/sizeof(MA_ODBC_TESTS) - 1;
  my_options= 67108866;
  get_options(argc, argv);
  plan(tests);
  mark_all_tests_normal(my_tests);

  return run_tests(my_tests);
}
