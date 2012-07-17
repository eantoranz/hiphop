/*
   +----------------------------------------------------------------------+
   | HipHop for PHP                                                       |
   +----------------------------------------------------------------------+
   | Copyright (c) 2010 Facebook, Inc. (http://www.facebook.com)          |
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

#include <test/test_ext_postgresql.h>
#include <runtime/ext/ext_postgresql.h>

IMPLEMENT_SEP_EXTENSION_TEST(Postgresql);
///////////////////////////////////////////////////////////////////////////////

bool TestExtPostgresql::RunTests(const std::string &which) {
  bool ret = true;

  RUN_TEST(test_pg_affected_rows);
  RUN_TEST(test_pg_cancel_query);
  RUN_TEST(test_pg_client_encoding);
  RUN_TEST(test_pg_close);
  RUN_TEST(test_pg_connect);
  RUN_TEST(test_pg_connection_busy);
  RUN_TEST(test_pg_connection_reset);
  RUN_TEST(test_pg_connection_status);
  RUN_TEST(test_pg_convert);
  RUN_TEST(test_pg_copy_from);
  RUN_TEST(test_pg_copy_to);
  RUN_TEST(test_pg_dbname);
  RUN_TEST(test_pg_delete);
  RUN_TEST(test_pg_end_copy);
  RUN_TEST(test_pg_escape_bytea);
  RUN_TEST(test_pg_escape_string);
  RUN_TEST(test_pg_execute);
  RUN_TEST(test_pg_fetch_all_columns);
  RUN_TEST(test_pg_fetch_all);
  RUN_TEST(test_pg_fetch_array);
  RUN_TEST(test_pg_fetch_assoc);
  RUN_TEST(test_pg_fetch_object);
  RUN_TEST(test_pg_fetch_result);
  RUN_TEST(test_pg_fetch_row);
  RUN_TEST(test_pg_field_is_null);
  RUN_TEST(test_pg_field_name);
  RUN_TEST(test_pg_field_num);
  RUN_TEST(test_pg_field_prtlen);
  RUN_TEST(test_pg_field_size);
  RUN_TEST(test_pg_field_table);
  RUN_TEST(test_pg_field_type_oid);
  RUN_TEST(test_pg_field_type);
  RUN_TEST(test_pg_free_result);
  RUN_TEST(test_pg_get_notify);
  RUN_TEST(test_pg_get_pid);
  RUN_TEST(test_pg_get_result);
  RUN_TEST(test_pg_host);
  RUN_TEST(test_pg_insert);
  RUN_TEST(test_pg_last_error);
  RUN_TEST(test_pg_last_notice);
  RUN_TEST(test_pg_last_oid);
  RUN_TEST(test_pg_lo_close);
  RUN_TEST(test_pg_lo_create);
  RUN_TEST(test_pg_lo_export);
  RUN_TEST(test_pg_lo_import);
  RUN_TEST(test_pg_lo_open);
  RUN_TEST(test_pg_lo_read_all);
  RUN_TEST(test_pg_lo_read);
  RUN_TEST(test_pg_lo_seek);
  RUN_TEST(test_pg_lo_tell);
  RUN_TEST(test_pg_lo_unlink);
  RUN_TEST(test_pg_lo_write);
  RUN_TEST(test_pg_meta_data);
  RUN_TEST(test_pg_num_fields);
  RUN_TEST(test_pg_num_rows);
  RUN_TEST(test_pg_options);
  RUN_TEST(test_pg_parameter_status);
  RUN_TEST(test_pg_pconnect);
  RUN_TEST(test_pg_ping);
  RUN_TEST(test_pg_port);
  RUN_TEST(test_pg_prepare);
  RUN_TEST(test_pg_put_line);
  RUN_TEST(test_pg_query_params);
  RUN_TEST(test_pg_query);
  RUN_TEST(test_pg_result_error_field);
  RUN_TEST(test_pg_result_error);
  RUN_TEST(test_pg_result_seek);
  RUN_TEST(test_pg_result_status);
  RUN_TEST(test_pg_select);
  RUN_TEST(test_pg_send_execute);
  RUN_TEST(test_pg_send_prepare);
  RUN_TEST(test_pg_send_query_params);
  RUN_TEST(test_pg_send_query);
  RUN_TEST(test_pg_set_client_encoding);
  RUN_TEST(test_pg_set_error_verbosity);
  RUN_TEST(test_pg_trace);
  RUN_TEST(test_pg_transaction_status);
  RUN_TEST(test_pg_tty);
  RUN_TEST(test_pg_unescape_bytea);
  RUN_TEST(test_pg_untrace);
  RUN_TEST(test_pg_update);
  RUN_TEST(test_pg_version);

  return ret;
}

///////////////////////////////////////////////////////////////////////////////

bool TestExtPostgresql::test_pg_affected_rows() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_cancel_query() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_client_encoding() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_close() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_connect() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_connection_busy() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_connection_reset() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_connection_status() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_convert() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_copy_from() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_copy_to() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_dbname() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_delete() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_end_copy() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_escape_bytea() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_escape_string() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_execute() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_fetch_all_columns() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_fetch_all() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_fetch_array() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_fetch_assoc() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_fetch_object() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_fetch_result() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_fetch_row() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_field_is_null() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_field_name() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_field_num() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_field_prtlen() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_field_size() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_field_table() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_field_type_oid() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_field_type() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_free_result() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_get_notify() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_get_pid() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_get_result() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_host() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_insert() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_last_error() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_last_notice() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_last_oid() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_lo_close() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_lo_create() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_lo_export() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_lo_import() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_lo_open() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_lo_read_all() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_lo_read() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_lo_seek() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_lo_tell() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_lo_unlink() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_lo_write() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_meta_data() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_num_fields() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_num_rows() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_options() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_parameter_status() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_pconnect() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_ping() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_port() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_prepare() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_put_line() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_query_params() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_query() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_result_error_field() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_result_error() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_result_seek() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_result_status() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_select() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_send_execute() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_send_prepare() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_send_query_params() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_send_query() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_set_client_encoding() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_set_error_verbosity() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_trace() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_transaction_status() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_tty() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_unescape_bytea() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_untrace() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_update() {
  return Count(true);
}

bool TestExtPostgresql::test_pg_version() {
  return Count(true);
}
