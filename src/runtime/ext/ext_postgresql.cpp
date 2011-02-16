/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010 Facebook, Inc. (http://www.facebook.com)          |
   | Copyright (c) 1997-2010 The PHP Group                                |
   +----------------------------------------------------------------------+
   | This source file is subject to version 3.01 of the PHP license,      |
   | that is bundled with this package in the file LICENSE, and is        |
   | available through the world-wide-web at the following url:           |
   | http://www.php.net/license/3_01.txt                                  |
   | If you did not receive a copy of the PHP license and are unable to   |
   | obtain it through the world-wide-web, please send a note to          |
   | license@php.net so we can mail you a copy immediately.               |
   +----------------------------------------------------------------------+
*/
/**
 * @TODO - change the column type map to load on demand, rather than always
 */
#include <runtime/ext/ext_postgresql.h>
#include <runtime/ext/ext_network.h>
#include <runtime/base/runtime_option.h>
#include <runtime/base/server/server_stats.h>
#include <runtime/base/util/request_local.h>
// #include <runtime/base/util/extended_logger.h>
#include <util/timer.h>
#include <netinet/in.h>
#include <netdb.h>

using namespace std;

namespace HPHP {
///////////////////////////////////////////////////////////////////////////////

StaticString Postgresql::s_class_name("postgresql link");
StaticString PostgresqlResult::s_class_name("postgresql result");

///////////////////////////////////////////////////////////////////////////////

IMPLEMENT_OBJECT_ALLOCATION_NO_DEFAULT_SWEEP(PostgresqlResult);

PostgresqlResult::PostgresqlResult(PGresult *res)
  : m_res(res) {
  //  m_fields = NULL;
  m_field_count = 0;
  m_current_row_num = 0;
  m_field_count = PQnfields(res);
  m_row_count = PQntuples(res);
}
int64 PostgresqlResult::nextRow()
{
  return m_current_row_num++;
}

int64 PostgresqlResult::numRows()
{
  return m_row_count;
}

int64 PostgresqlResult::numFields()
{
  return m_field_count;
}

PostgresqlResult::~PostgresqlResult() {
  close();
}

void PostgresqlResult::sweep() {
  close();
  // When a dangling PostgresqlResult is swept, there is no need to deallocate
  // any Variant object.
}

///////////////////////////////////////////////////////////////////////////////

class PostgresqlStaticInitializer {
public:
  PostgresqlStaticInitializer() {

  }
};
static PostgresqlStaticInitializer s_postgresql_initializer;

class PostgresqlRequestData : public RequestEventHandler {
public:
  virtual void requestInit() {
    defaultConn.reset();
    //    readTimeout = RuntimeOption::PostgresqlReadTimeout;
    totalRowCount = 0;
  }

  virtual void requestShutdown() {
    defaultConn.reset();
    totalRowCount = 0;
  }

  Object defaultConn;
  int readTimeout;
  int totalRowCount; // from all queries in current request
};
IMPLEMENT_STATIC_REQUEST_LOCAL(PostgresqlRequestData, s_postgresql_data);

///////////////////////////////////////////////////////////////////////////////
// class Postgresql statics

int Postgresql::s_default_port = 0;

Postgresql *Postgresql::Get(CVarRef link_identifier) {

  if (link_identifier.isNull()) {
    return GetDefaultConn();
  }
  /*  Postgresql *postgresql = link_identifier.toObject().getTyped<Postgresql>
    (!RuntimeOption::ThrowBadTypeExceptions,
    !RuntimeOption::ThrowBadTypeExceptions); */
  Postgresql *postgresql = link_identifier.toObject().getTyped<Postgresql>(true, true); 
  if (postgresql == NULL) {
    raise_warning("Failed to object");
  }
  return postgresql;
}

PGconn *Postgresql::GetConn(CVarRef link_identifier, Postgresql **rconn /* = NULL */) {
  Postgresql *postgresql = Get(link_identifier);
  PGconn *ret = NULL;
  if (postgresql) {
    ret = postgresql->get();
  }
  if (ret == NULL) {
    raise_warning("supplied argument is not a valid Postgresql-Link resource");
  }
  if (rconn) {
    *rconn = postgresql;
  }
  return ret;
}

bool Postgresql::CloseConn(CVarRef link_identifier) {
  Postgresql *postgresql = Get(link_identifier);
  if (postgresql) {
    postgresql->close();
  }
  return true;
}

int Postgresql::GetDefaultPort() {
  if (s_default_port <= 0) {
    s_default_port = POSTGRESQL_PORT;
    char *env = getenv("POSTGRESQL_TCP_PORT");
    if (env && *env) {
      s_default_port = atoi(env);
    } else {
      Variant ret = f_getservbyname("postgresql", "tcp");
      if (!same(ret, false)) {
        s_default_port = ret.toInt16();
      }
    }
  }
  return s_default_port;
}

String Postgresql::GetDefaultSocket() {
  return POSTGRESQL_UNIX_ADDR;
}

String Postgresql::GetHash(CStrRef dsn) {
  char buf[1024];
  snprintf(buf, sizeof(buf), "%s",
           dsn.data());
  return String(buf, CopyString);
}

Postgresql *Postgresql::GetCachedImpl(const char *name, CStrRef dsn) {
  String key = GetHash(dsn);
  return dynamic_cast<Postgresql*>(g_persistentObjects->get(name, key.data()));
}

void Postgresql::SetCachedImpl(const char *name, CStrRef dsn, Postgresql *conn) {
  String key = GetHash(dsn);
  g_persistentObjects->set(name, key.data(), conn);
}

Postgresql *Postgresql::GetDefaultConn() {
  return s_postgresql_data->defaultConn.getTyped<Postgresql>(true);
}

void Postgresql::SetDefaultConn(Postgresql *conn) {
  s_postgresql_data->defaultConn = conn;
}

///////////////////////////////////////////////////////////////////////////////
// class Postgresql

Postgresql::Postgresql(const char *dsn)
 {
  m_dsn = dsn;
  m_conn = NULL;
  m_type_map_loaded = false;
}

Postgresql::~Postgresql() {
  close();
}

void Postgresql::setLastError(const char *func) {
  ASSERT(m_conn);
  m_last_error_set = true;
  const char *error = PQerrorMessage(m_conn);
  m_last_error = error ? error : "";
  raise_warning("%s(): %s", func, m_last_error.c_str());
}

void Postgresql::close() {
  if (m_conn) {
    m_last_error_set = false;
    m_xaction_count = 0;
    m_last_error.clear();
    PQfinish(m_conn);
    m_conn = NULL;
  }
}

String Postgresql::GetTypeName(Oid oid)
{
  if (m_type_map_loaded == false) {
    load_type_map();
  }
  return String(m_types.find(oid)->second, CopyString);
}

bool Postgresql::connect(CStrRef dsn) {
  // @TODO: implement options here
  if (RuntimeOption::EnableStats && RuntimeOption::EnableSQLStats) {
    ServerStats::Log("sql.conn", 1);
  }
  IOStatusHelper io("postgresql::connect", dsn.data());
  m_xaction_count = 0;
  m_conn = PQconnectdb(dsn.data());
  bool ret;
  if (m_conn==NULL || PQstatus(m_conn)==CONNECTION_BAD) {
    PQfinish(m_conn);
    m_conn=NULL;
    ret = false;
  }

  ret = true;
  // @TODO: set post connect options here
  /*      raise_notice("Postgresql::connect: failed setting session wait timeout: %s",
          postgresql_error(m_conn));*/
  return ret;
}

void Postgresql::load_type_map()
{
  m_type_map_loaded = true;
  PGresult   *res;
  
  // get types as test
  res = PQexec(m_conn, "select oid,typname from pg_type");
  
  int nRows = PQntuples(res);
  
  int j;
  for (j = 0; j < nRows; j++) {
    Oid oid = atoi(PQgetvalue(res, j, 0));
    m_types.insert(std::pair<Oid, String>(oid, String(PQgetvalue(res, j, 1), CopyString)));  
  }
}

bool Postgresql::reconnect(CStrRef dsn) {
  if (m_conn == NULL) {
    m_conn = PQconnectdb(dsn.data());
    int connect_timeout = 0; // @TODO: implement
    if (connect_timeout >= 0) {
      /*      PostgresqlUtil::set_postgresql_timeout(m_conn, PostgresqlUtil::ConnectTimeout,
              connect_timeout); */
    }
    if (RuntimeOption::EnableStats && RuntimeOption::EnableSQLStats) {
      ServerStats::Log("sql.reconn_new", 1);
    }
    IOStatusHelper io("postgresql::connect", dsn.data());
    if (m_conn==NULL || PQstatus(m_conn)==CONNECTION_BAD) {
      PQfinish(m_conn);
      m_conn = NULL;
      return false;
    }
    return true;
  }

  PGresult   *res;
  // select 1
  res = PQexec(m_conn, "select 1");
  PQclear(res);
  if (PQstatus(m_conn) == CONNECTION_OK) {
    if (RuntimeOption::EnableStats && RuntimeOption::EnableSQLStats) {
      ServerStats::Log("sql.reconn_ok", 1);
    }
    return true;
  }

  if (RuntimeOption::EnableStats && RuntimeOption::EnableSQLStats) {
    ServerStats::Log("sql.reconn_old", 1);
  }
  IOStatusHelper io("postgresql::connect", dsn.data());
  m_xaction_count = 0;
  m_conn = PQconnectdb(dsn.data());
  if (m_conn==NULL || PQstatus(m_conn)==CONNECTION_BAD) {
    PQfinish(m_conn);
    m_conn = NULL;
    return false;
  }
  return true;
}

///////////////////////////////////////////////////////////////////////////////
// helpers

static PostgresqlResult *get_result(CVarRef result) {
  PostgresqlResult *res = result.toObject().getTyped<PostgresqlResult>
    (!RuntimeOption::ThrowBadTypeExceptions,
     !RuntimeOption::ThrowBadTypeExceptions);
  if (res == NULL || (res->get() == NULL)) {
    raise_warning("supplied argument is not a valid Postgresql result resource");
  }
  return res;
}

#define PHP_POSTGRESQL_FIELD_NAME  1
#define PHP_POSTGRESQL_FIELD_TABLE 2
#define PHP_POSTGRESQL_FIELD_LEN   3
#define PHP_POSTGRESQL_FIELD_TYPE  4
#define PHP_POSTGRESQL_FIELD_FLAGS 5

  static Variant php_postgresql_do_connect(String dsn, int connection_type, bool persistent) {

    // @todo: implement connection_type

  Object ret;
  Postgresql *postgresql = NULL;

  if (persistent && connection_type != k_PGSQL_CONNECT_FORCE_NEW) {
    postgresql = Postgresql::GetPersistent(dsn);
  }

  if (postgresql == NULL) {
    postgresql = new Postgresql(dsn);
    if (!postgresql->connect(dsn)) {
      raise_warning("Not connect");
      Postgresql::SetDefaultConn(postgresql); // so we can report errno by postgresql_errno()
      postgresql->setLastError("postgresql_connect");
      return false;
    }
    ret = postgresql;
  } else {
    raise_warning("Not null");
    ret = postgresql;
    if (!postgresql->reconnect(dsn)) {
      Postgresql::SetDefaultConn(postgresql); // so we can report errno by postgresql_errno()
      postgresql->setLastError("postgresql_connect");
      return false;
    }
  }

  if (persistent) {
    Postgresql::SetPersistent(dsn, postgresql);
  }
  Postgresql::SetDefaultConn(postgresql);
  return ret;
}

///////////////////////////////////////////////////////////////////////////////


int64 f_pg_affected_rows(CVarRef result) {

  PostgresqlResult *resObj = get_result(result);
  if (resObj == NULL) {
    //    raise_warning('
    return 0;   
  }

  PGresult *res = resObj->get();

  return atoi(PQcmdTuples(res));
}

void f_pg_cancel_query(CVarRef connection) {
  /*
PQgetCancel

    Creates a data structure containing the information needed to cancel a command issued through a particular database connection.

    PGcancel *PQgetCancel(PGconn *conn);

    PQgetCancel creates a PGcancel object given a PGconn connection object. It will return NULL if the given conn is NULL or an invalid connection. The PGcancel object is an opaque structure that is not meant to be accessed directly by the application; it can only be passed to PQcancel or PQfreeCancel. 
PQfreeCancel

    Frees a data structure created by PQgetCancel.

    void PQfreeCancel(PGcancel *cancel);

    PQfreeCancel frees a data object previously created by PQgetCancel. 
PQcancel

    Requests that the server abandon processing of the current command.

    int PQcancel(PGcancel *cancel, char *errbuf, int errbufsize);

    The return value is 1 if the cancel request was successfully dispatched and 0 if not. If not, errbuf is filled with an error message explaining why not. errbuf must be a char array of size errbufsize (the recommended size is 256 bytes).

    Successful dispatch is no guarantee that the request will have any effect, however. If the cancellation is effective, the current command will terminate early and return an error result. If the cancellation fails (say, because the server was already done processing the command), then there will be no visible result at all.

    PQcancel can safely be invoked from a signal handler, if the errbuf is a local variable in the signal handler. The PGcancel object is read-only as far as PQcancel is concerned, so it can also be invoked from a thread that is separate from the one manipulating the PGconn object. 
  */
  throw NotImplementedException(__func__);
}

void f_pg_client_encoding(CStrRef connection) {
  throw NotImplementedException(__func__);
}

void f_pg_close(CVarRef connection) {
  throw NotImplementedException(__func__);
}

Variant f_pg_connect(CStrRef connection_string, int connect_type) {
  return php_postgresql_do_connect(connection_string, connect_type, false);
}

Variant f_pg_connect(CStrRef connection_string) {
  return php_postgresql_do_connect(connection_string, 0, false);
}

void f_pg_connection_busy(CVarRef connection) {
  /*

    PQisBusy

    Returns 1 if a command is busy, that is, PQgetResult would block waiting for input. A 0 return indicates that PQgetResult can be called with assurance of not blocking.

    int PQisBusy(PGconn *conn);

    PQisBusy will not itself attempt to read data from the server; therefore PQconsumeInput must be invoked first, or the busy state will never end. 

   */
  throw NotImplementedException(__func__);
}

void f_pg_connection_reset(CVarRef connection) {
  throw NotImplementedException(__func__);
}

void f_pg_connection_status(CVarRef connection) {
  /*
PQstatus

    Returns the status of the connection.

    ConnStatusType PQstatus(const PGconn *conn);

    The status can be one of a number of values. However, only two of these are seen outside of an asynchronous connection procedure: CONNECTION_OK and CONNECTION_BAD. A good connection to the database has the status CONNECTION_OK. A failed connection attempt is signaled by status CONNECTION_BAD. Ordinarily, an OK status will remain so until PQfinish, but a communications failure might result in the status changing to CONNECTION_BAD prematurely. In that case the application could try to recover by calling PQreset.

    See the entry for PQconnectStartParams, PQconnectStart and PQconnectPoll with regards to other status codes that might be seen. 
  */
  throw NotImplementedException(__func__);
}

void f_pg_convert(CVarRef connection, CStrRef table_name, CStrRef assoc_array, CStrRef options) {
  throw NotImplementedException(__func__);
}

void f_pg_copy_from(CVarRef connection, CStrRef table_name, CStrRef rows, CStrRef delimiter, CStrRef null_as) {
  /*
use PQgetCopyData?

    Receives data from the server during COPY_OUT state.
   */
  throw NotImplementedException(__func__);
}

void f_pg_copy_to(CVarRef connection, CStrRef table_name, CStrRef rows, CStrRef delimiter, CStrRef null_as) {
  /*

use PQputCopyData ?

    Sends data to the server during COPY_IN state.

    int PQputCopyData(PGconn *conn,
                      const char *buffer,
                      int nbytes);
   */
  throw NotImplementedException(__func__);
}

void f_pg_dbname(CStrRef connection) {
  /*
PQdb

    Returns the database name of the connection.

    char *PQdb(const PGconn *conn);
  */
  throw NotImplementedException(__func__);
}

void f_pg_delete(CVarRef connection, CStrRef table_name, CStrRef assoc_array, CStrRef options) {
  throw NotImplementedException(__func__);
}

void f_pg_end_copy(CVarRef connection) {
  /*
// pqendcopy is deprecated
// @TODO switch to PQputCopyEnd 
PQendcopy

    Synchronizes with the server.

    int PQendcopy(PGconn *conn);

   */
  throw NotImplementedException(__func__);
}

Variant f_pg_escape_bytea(CVarRef connection, CStrRef data) {

    Postgresql *rconn = NULL;
  PGconn *conn = Postgresql::GetConn(connection, &rconn);

  if (!conn || !rconn) {
    return false;
  }
  size_t to_length;
  unsigned char *to;
  String ret;
  to = PQescapeByteaConn(conn, (unsigned char *) data.data(), data.length(), &to_length);
  ret = String((char *) to, CopyString);
  PQfreemem(to);
  return ret;
}

Variant f_pg_escape_string(CVarRef connection, CStrRef data) {
  Postgresql *rconn = NULL;
  PGconn *conn = Postgresql::GetConn(connection, &rconn);

  if (!conn || !rconn) {
    raise_warning("No postgresql connection, can't escape string");
    return false;
  }
  int escape_error;
  size_t ret_length;
  char *to;
  to = (char *)malloc(data.length()*2+1);
  ret_length = PQescapeStringConn(conn, to, data.data(), data.length(),  &escape_error);
  return String(to, ret_length, AttachString);
}


void f_pg_execute(CVarRef connection, CStrRef stmtname, CArrRef params) {
  throw NotImplementedException(__func__);
}

Variant f_pg_fetch_all_columns(CVarRef result, CStrRef column) {
  throw NotImplementedException(__func__);
}

Variant f_pg_fetch_all(CVarRef result) {
  throw NotImplementedException(__func__);
}

#define POSTGRESQL_ASSOC  1 << 0
#define POSTGRESQL_NUM    1 << 1
#define POSTGRESQL_BOTH   (POSTGRESQL_ASSOC|POSTGRESQL_NUM)


static Variant postgresql_makevalue(CStrRef data, Oid postgresql_field) {
  // @TODO: use constants if they exist  
  switch (postgresql_field) {
  case 20:
  case 21:
  case 22:
  case 23:
  case 24:
    return data.toInt64();
  case 1700:
    return data.toDouble();
    break;
  default:
    break;
  }
  return data;
}


static Variant php_postgresql_fetch_hash(CVarRef result, CVarRef row, int result_type) {
  if ((result_type & POSTGRESQL_BOTH) == 0) {
    throw_invalid_argument("result_type: %d", result_type);
    return false;
  }
  int64 row_num;
  PostgresqlResult *resObj = get_result(result);
  if (resObj == NULL) return false;

  if (row.isNull()) {
    row_num = resObj->nextRow();
  } else {
    row_num = row.toInt64();
  }

  if (row_num < 0) {
    raise_warning("Invalid row number");
    return false;
  }

  Array ret;
  PGresult *res = resObj->get();
  int nFields, j;


  if (row_num >= PQntuples(res)) {
    return false;
  }
  nFields = PQnfields(res);
  for (j = 0; j < nFields; j++) {
    Variant data;
    int is_null;
    is_null = PQgetisnull(res, row_num, j);
    if (is_null == 1) {
      data = null;
    } else {
      data =  postgresql_makevalue(String(PQgetvalue(res, row_num, j), PQgetlength(res, row_num, j), CopyString), PQftype(res, j));
    }
    if (result_type & POSTGRESQL_NUM) {
      ret.set(j, data);
    }
    if (result_type & POSTGRESQL_ASSOC) {
      ret.set(String(PQfname(res, j), CopyString), data);
    }
  }

  return ret;
}


Variant f_pg_fetch_array(CVarRef result, CVarRef row, int result_type) {
  return php_postgresql_fetch_hash(result, row, result_type);
}

Variant f_pg_fetch_assoc(CVarRef result, CVarRef row) /* = null */ {
  return php_postgresql_fetch_hash(result, row, POSTGRESQL_ASSOC);
}

Variant f_pg_fetch_object(CVarRef result, CVarRef row /* = null */, CStrRef class_name /* = null */, CVarRef params /* = null */) {
  Variant properties = php_postgresql_fetch_hash(result, row, POSTGRESQL_ASSOC);
  if (!same(properties, false)) {
    Object obj = create_object(class_name.data(), params);
    obj->o_setArray(properties);
    return obj;
  }
  return false;
}

Variant f_pg_fetch_result(CVarRef result, int64 row, CStrRef field) {
  throw NotImplementedException(__func__);
}

Variant f_pg_fetch_row(CVarRef result, CVarRef row /* = null */) {
  return php_postgresql_fetch_hash(result, row, POSTGRESQL_NUM);
}

Variant f_pg_field_is_null(CVarRef result, int64 row, CStrRef field) {
  throw NotImplementedException(__func__);
}

String f_pg_field_name(CVarRef result, int64 field_number) {
  PostgresqlResult *resObj = get_result(result);
  if (resObj == NULL) {
    return false;
  }
  PGresult *res = resObj->get();
  if (resObj == NULL) {
    return false;
  }

  return String(PQfname(res, field_number), CopyString);
}

Variant f_pg_field_num(CVarRef result, CStrRef field_name) {
  /*
PQfnumber

    Returns the column number associated with the given column name.

    int PQfnumber(const PGresult *res,
                  const char *column_name);

    -1 is returned if the given name does not match any column. 
  */
  throw NotImplementedException(__func__);
}

Variant f_pg_field_prtlen(CVarRef result, CVarRef field_name_or_number) {
  throw NotImplementedException(__func__);
}

Variant f_pg_field_size(CVarRef result, int64 field_number) {
  /*
PQfsize

    Returns the size in bytes of the column associated with the given column number. Column numbers start at 0.

    int PQfsize(const PGresult *res,
                int column_number);

    PQfsize returns the space allocated for this column in a database row, in other words the size of the server's internal representation of the data type. (Accordingly, it is not really very useful to clients.) A negative value indicates the data type is variable-length. 
  */
  throw NotImplementedException(__func__);
}

String f_pg_field_table(CVarRef result, int64 field_number, CStrRef oid_only) {
  /*
PQftable

    Returns the OID of the table from which the given column was fetched. Column numbers start at 0.

    Oid PQftable(const PGresult *res,
                 int column_number);

    InvalidOid is returned if the column number is out of range, or if the specified column is not a simple reference to a table column, or when using pre-3.0 protocol. You can query the system table pg_class to determine exactly which table is referenced.

    The type Oid and the constant InvalidOid will be defined when you include the libpq header file. They will both be some integer type. 
  */
  throw NotImplementedException(__func__);
}

String f_pg_field_type_oid(CVarRef result, int64 field_number) {
  PostgresqlResult *resObj = get_result(result);
  if (resObj == NULL) {
    return false;
  }
  PGresult *res = resObj->get();
  if (resObj == NULL) {
    return false;
  }

  Oid oid;
  oid = PQftype(res, field_number);
  char buf[255];
  snprintf(buf, sizeof(buf), "%i", oid);
  return String(buf, CopyString);
}

String f_pg_field_type(CVarRef result, int64 field_number) {
  PostgresqlResult *resObj = get_result(result);
  if (resObj == NULL) {
    return false;
  }
  PGresult *res = resObj->get();
  if (resObj == NULL) {
    return false;
  }

  Oid oid;
  oid = PQftype(res, field_number);

  Postgresql *postgresql = Postgresql::GetDefaultConn();

  return postgresql->GetTypeName(oid);
}

Variant f_pg_free_result(CVarRef result) {
  PostgresqlResult *resObj = get_result(result);
  if (resObj == NULL) {
    return false;
  }
  resObj->close();
  return true;
}

Variant f_pg_get_notify(CVarRef connection, int result_type) {
  /*
PGnotify *PQnotifies(PGconn *conn);
 ?
   */
  throw NotImplementedException(__func__);
}

String f_pg_get_pid(CVarRef connection) {
  /*
PQbackendPID

    Returns the process ID (PID) of the backend server process handling this connection.

    int PQbackendPID(const PGconn *conn);

    The backend PID is useful for debugging purposes and for comparison to NOTIFY messages (which include the PID of the notifying backend process). Note that the PID belongs to a process executing on the database server host, not the local host! 

  */
  throw NotImplementedException(__func__);
}

Variant f_pg_get_result(CVarRef connection) {
  /*

PQgetResult

    Waits for the next result from a prior PQsendQuery, PQsendQueryParams, PQsendPrepare, or PQsendQueryPrepared call, and returns it. A null pointer is returned when the command is complete and there will be no more results.

    PGresult *PQgetResult(PGconn *conn);

    PQgetResult must be called repeatedly until it returns a null pointer, indicating that the command is done. (If called when no command is active, PQgetResult will just return a null pointer at once.) Each non-null result from PQgetResult should be processed using the same PGresult accessor functions previously described. Don't forget to free each result object with PQclear when done with it. Note that PQgetResult will block only if a command is active and the necessary response data has not yet been read by PQconsumeInput. 


   */
  throw NotImplementedException(__func__);
}

String f_pg_host(CVarRef connection) {
  /*
PQhost

    Returns the server host name of the connection.

    char *PQhost(const PGconn *conn);
  */
  throw NotImplementedException(__func__);
}

Variant f_pg_insert(CVarRef connection, CStrRef table_name, CStrRef assoc_array, CStrRef options) {
  throw NotImplementedException(__func__);
}

String f_pg_last_error(CVarRef connection) {
 Postgresql *postgresql = Postgresql::Get(connection);
  if (!postgresql) {
    raise_warning("supplied argument is not a valid MySQL-Link resource");
    return false;
  }
  PGconn *conn = postgresql->get();
  if (conn) {
    return String(PQerrorMessage(conn), CopyString);
  }
  if (postgresql->m_last_error_set) {
    return String(postgresql->m_last_error);
  }
  return false;
}

String f_pg_last_notice(CVarRef connection) {
  throw NotImplementedException(__func__);
}

String f_pg_last_oid(CVarRef result) {
  throw NotImplementedException(__func__);
}

Variant f_pg_lo_close(CVarRef result) {
  throw NotImplementedException(__func__);
}

Variant f_pg_lo_create(CVarRef connection, CStrRef object_id) {
  throw NotImplementedException(__func__);
}

Variant f_pg_lo_export(CVarRef connection, CStrRef oid, CStrRef pathname) {
  throw NotImplementedException(__func__);
}

Variant f_pg_lo_import(CVarRef connection, CStrRef pathname, CStrRef object_id) {
  throw NotImplementedException(__func__);
}

Variant f_pg_lo_open(CVarRef connection, CStrRef oid, CStrRef mode) {
  throw NotImplementedException(__func__);
}

Variant f_pg_lo_read_all(CStrRef large_object) {
  throw NotImplementedException(__func__);
}

Variant f_pg_lo_read(CStrRef large_object, CStrRef len) {
  throw NotImplementedException(__func__);
}

Variant f_pg_lo_seek(CStrRef large_object, CStrRef offset, CStrRef whence) {
  throw NotImplementedException(__func__);
}

Variant f_pg_lo_tell(CStrRef large_object) {
  throw NotImplementedException(__func__);
}

Variant f_pg_lo_unlink(CVarRef connection, CStrRef oid) {
  throw NotImplementedException(__func__);
}

Variant f_pg_lo_write(CStrRef large_object, CStrRef data, CStrRef len) {
  throw NotImplementedException(__func__);
}

Variant f_pg_meta_data(CVarRef connection, CStrRef table_name) {
  throw NotImplementedException(__func__);
}

Variant f_pg_num_fields(CVarRef result) {
  PostgresqlResult *resObj = get_result(result);
  if (resObj == NULL) {
    return false;
  }
  return resObj->numFields();
}

Variant f_pg_num_rows(CVarRef result) {
  PostgresqlResult *resObj = get_result(result);
  if (resObj == NULL) {
    return false;
  }
  return resObj->numRows();
}

Variant f_pg_options(CVarRef connection) {
  throw NotImplementedException(__func__);
}

String f_pg_parameter_status(CVarRef connection, CStrRef param_name) {
  /*
PQparameterStatus

    Looks up a current parameter setting of the server.

    const char *PQparameterStatus(const PGconn *conn, const char *paramName);

    Certain parameter values are reported by the server automatically at connection startup or whenever their values change. PQparameterStatus can be used to interrogate these settings. It returns the current value of a parameter if known, or NULL if the parameter is not known.

    Parameters reported as of the current release include server_version, server_encoding, client_encoding, application_name, is_superuser, session_authorization, DateStyle, IntervalStyle, TimeZone, integer_datetimes, and standard_conforming_strings. (server_encoding, TimeZone, and integer_datetimes were not reported by releases before 8.0; standard_conforming_strings was not reported by releases before 8.1; IntervalStyle was not reported by releases before 8.4; application_name was not reported by releases before 9.0.) Note that server_version, server_encoding and integer_datetimes cannot change after startup.

    Pre-3.0-protocol servers do not report parameter settings, but libpq includes logic to obtain values for server_version and client_encoding anyway. Applications are encouraged to use PQparameterStatus rather than ad hoc code to determine these values. (Beware however that on a pre-3.0 connection, changing client_encoding via SET after connection startup will not be reflected by PQparameterStatus.) For server_version, see also PQserverVersion, which returns the information in a numeric form that is much easier to compare against.

    If no value for standard_conforming_strings is reported, applications can assume it is off, that is, backslashes are treated as escapes in string literals. Also, the presence of this parameter can be taken as an indication that the escape string syntax (E'...') is accepted.

    Although the returned pointer is declared const, it in fact points to mutable storage associated with the PGconn structure. It is unwise to assume the pointer will remain valid across queries. 
   */
  throw NotImplementedException(__func__);
}

Variant f_pg_pconnect(CStrRef connection_string, CStrRef host, CStrRef hostaddr, CStrRef port, CStrRef dbname, CStrRef user, CStrRef password, CStrRef connect_timeout, CStrRef options, CStrRef tty, CStrRef sslmode, CStrRef requiressl, CStrRef service, CStrRef connect_type) {
  throw NotImplementedException(__func__);
}

Variant f_pg_ping(CVarRef connection) {
  throw NotImplementedException(__func__);
}

Variant f_pg_port(CVarRef connection) {
  /*
PQport

    Returns the port of the connection.

    char *PQport(const PGconn *conn);
  */
  throw NotImplementedException(__func__);
}

Variant f_pg_prepare(CVarRef connection, CStrRef stmtname, CStrRef query) {
  throw NotImplementedException(__func__);
}

Variant f_pg_put_line(CVarRef connection, CStrRef data) {
  throw NotImplementedException(__func__);
}

Variant f_pg_query_params(CVarRef connection, CStrRef query, CArrRef params) {
  int param_count;
  const char * *paramValues;
  Postgresql *rconn = NULL;
  PGconn *conn = Postgresql::GetConn(connection, &rconn);
  PGresult   *res;

  if (!conn || !rconn) {
    return NULL;
  }
  
  param_count = params.size();
  paramValues = (const char **) calloc(param_count, sizeof (char *));

  int i = 0;
  for (ArrayIter iter(params); iter; ++iter) {
    Variant arg = iter.second().toString();

    if (arg.getType() == KindOfNull) {
      paramValues[i] =  NULL;
      // handling other types like bool would be nice here, but PHP does not do this
    } else {
      char *arg_ptr;
      String arg_string = arg.toString();
      arg_ptr = (char *) malloc(arg_string.length()+1);
      strncpy(arg_ptr, arg_string.data(), arg_string.length()+1);
      paramValues[i] = arg_ptr;
    }
    i++;
  }

  res = PQexecParams(conn, query.data(), param_count, NULL, paramValues, NULL, NULL, 0);

  free(paramValues);

  if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK)
    {
      char *status_string;
      status_string = PQresStatus(PQresultStatus(res));
      raise_warning(status_string);
      PQclear(res);
      return NULL;
    }
  Variant result = Object(NEW(PostgresqlResult)(res));
  return result;
}

Variant f_pg_query(CVarRef connection, CStrRef query) {
  Postgresql *rconn = NULL;
  PGconn *conn = Postgresql::GetConn(connection, &rconn);
  PGresult   *res;

  if (!conn || !rconn) {
    return false;
  }
  res = PQexec(conn, query.data());
  if (PQresultStatus(res) != PGRES_COMMAND_OK && PQresultStatus(res) != PGRES_TUPLES_OK)
    {
      char *status_string;
      status_string = PQresStatus(PQresultStatus(res));
      raise_warning(status_string);
      //      fprintf(stderr, "BEGIN command failed: %s", PQerrorMessage(conn));
      PQclear(res);
      return NULL;
    }
  Variant result = Object(NEW(PostgresqlResult)(res));
  return result;
}

Variant f_pg_result_error_field(CVarRef result, CStrRef fieldcode) {
  throw NotImplementedException(__func__);
}

Variant f_pg_result_error(CVarRef result) {
  throw NotImplementedException(__func__);
}

Variant f_pg_result_seek(CVarRef result, CStrRef offset) {
  throw NotImplementedException(__func__);
}

Variant f_pg_result_status(CVarRef result, int64 type) {
  // type values
  // PGSQL_STATUS_LONG = 1 default
  // PGSQL_STATUS_STRING = 2
  // return values for LONG:
  //  PGSQL_EMPTY_QUERY, PGSQL_COMMAND_OK, PGSQL_TUPLES_OK, PGSQL_COPY_OUT, PGSQL_COPY_IN, PGSQL_BAD_RESPONSE, PGSQL_NONFATAL_ERROR and PGSQL_FATAL_ERROR 
  PostgresqlResult *resObj = get_result(result);
  if (resObj == NULL) {
    return false;
  }
  PGresult *res = resObj->get();
  if (resObj == NULL) {
    return false;
  }

  if (type == 2) {
    // PGSQL_STATUS_STRING
    return String(PQresStatus(PQresultStatus(res)), CopyString);    
  } else { 
    return PQresultStatus(res);
  }

}

Variant f_pg_select(CVarRef connection, CStrRef table_name, CStrRef assoc_array, CStrRef options) {
  throw NotImplementedException(__func__);
}

Variant f_pg_send_execute(CVarRef connection, CStrRef stmtname, CArrRef params) {
  /*
PQsendQueryPrepared

    Sends a request to execute a prepared statement with given parameters, without waiting for the result(s).

    int PQsendQueryPrepared(PGconn *conn,
                            const char *stmtName,
                            int nParams,
                            const char * const *paramValues,
                            const int *paramLengths,
                            const int *paramFormats,
                            int resultFormat);

  */
  throw NotImplementedException(__func__);
}

Variant f_pg_send_prepare(CVarRef connection, CStrRef stmtname, CStrRef query) {
  /*
PQsendPrepare

    Sends a request to create a prepared statement with the given parameters, without waiting for completion.

    int PQsendPrepare(PGconn *conn,
                      const char *stmtName,
                      const char *query,
                      int nParams,
                      const Oid *paramTypes);
  */
  throw NotImplementedException(__func__);
}

Variant f_pg_send_query_params(CVarRef connection, CStrRef query, CArrRef params) {
  /*
PQsendQueryParams

    Submits a command and separate parameters to the server without waiting for the result(s).

    int PQsendQueryParams(PGconn *conn,
                          const char *command,
                          int nParams,
                          const Oid *paramTypes,
                          const char * const *paramValues,
                          const int *paramLengths,
                          const int *paramFormats,
                          int resultFormat);
  */
  throw NotImplementedException(__func__);
}

Variant f_pg_send_query(CVarRef connection, CStrRef query) {
  /*
    PQsendQuery

    Submits a command to the server without waiting for the result(s). 1 is returned if the command was successfully dispatched and 0 if not (in which case, use PQerrorMessage to get more information about the failure).

    int PQsendQuery(PGconn *conn, const char *command);

    After successfully calling PQsendQuery, call PQgetResult one or more times to obtain the results. PQsendQuery cannot be called again (on the same connection) until PQgetResult has returned a null pointer, indicating that the command is done. 
   */
  throw NotImplementedException(__func__);
}

Variant f_pg_set_client_encoding(CVarRef connection, CStrRef encoding) {
  throw NotImplementedException(__func__);
}

Variant f_pg_set_error_verbosity(CVarRef connection, CStrRef verbosity) {
  throw NotImplementedException(__func__);
}

bool f_pg_trace(CStrRef pathname, CStrRef mode /* = "w" */, CVarRef connection /* = null */) {
  throw NotImplementedException(__func__);
}

int f_pg_transaction_status(CVarRef connection) {
  /*
PQtransactionStatus

    Returns the current in-transaction status of the server.

    PGTransactionStatusType PQtransactionStatus(const PGconn *conn);

    The status can be PQTRANS_IDLE (currently idle), PQTRANS_ACTIVE (a command is in progress), PQTRANS_INTRANS (idle, in a valid transaction block), or PQTRANS_INERROR (idle, in a failed transaction block). PQTRANS_UNKNOWN is reported if the connection is bad. PQTRANS_ACTIVE is reported only when a query has been sent to the server and not yet completed.

  */
  throw NotImplementedException(__func__);
}

String f_pg_tty(CVarRef connection) {
  /*
PQtransactionStatus

    Returns the current in-transaction status of the server.

    PGTransactionStatusType PQtransactionStatus(const PGconn *conn);

    The status can be PQTRANS_IDLE (currently idle), PQTRANS_ACTIVE (a command is in progress), PQTRANS_INTRANS (idle, in a valid transaction block), or PQTRANS_INERROR (idle, in a failed transaction block). PQTRANS_UNKNOWN is reported if the connection is bad. PQTRANS_ACTIVE is reported only when a query has been sent to the server and not yet completed.

  */
  throw NotImplementedException(__func__);
}

String f_pg_unescape_bytea(CStrRef data) {
  size_t to_length;
  unsigned char *to;
  String ret;
  to = PQunescapeBytea((unsigned char *) data.data(), &to_length);
  ret = String((char *) to, to_length, CopyString);
  PQfreemem(to);
  return ret;
}

bool f_pg_untrace(CVarRef connection) {
  throw NotImplementedException(__func__);
}

Variant f_pg_update(CVarRef connection, CStrRef table_name, CVarRef data, CVarRef condition, int options) {
  throw NotImplementedException(__func__);
}

Variant f_pg_version(CVarRef connection) {
  Postgresql *rconn = NULL;
  PGconn *conn = Postgresql::GetConn(connection, &rconn);
  Array ret;

  int server_version = PQserverVersion(conn);

  char buf[1024];
  int major;
  int minor;
  int revision;
  revision = server_version % 100;
  server_version -= revision;
  server_version /= 100;
  minor = server_version % 100;
  server_version -= minor;
  server_version /= 100;
  major = server_version;
  
  snprintf(buf, sizeof(buf), "%i.%i.%i", major, minor, revision);
  ret.set("server", String(buf, CopyString));

  ret.set("protocol", PQprotocolVersion(conn));
  ret.set("client", PG_VERSION);
  return ret;
}


///////////////////////////////////////////////////////////////////////////////
}
