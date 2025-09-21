#ifndef ODBC_MODULE_H
#define ODBC_MODULE_H

#include <sql.h>
#include <string>
#include <iostream>
#include "../Parsers/call_state_parser.h"

class PostgreSQLConnector 
{
private:
    SQLHENV henv;           ///< Handle окружения ODBC
    SQLHDBC hdbc;           ///< Handle соединения с базой данных
    SQLHSTMT hstmt;         ///< Handle для выполнения SQL statements
    SQLRETURN ret;          ///< Код возврата ODBC функций
    bool transactionActive; ///< Флаг активности транзакции
    bool isConnected;       ///< Флаг подключения к БД

public:

    PostgreSQLConnector();
    ~PostgreSQLConnector();
    bool connectToServer();
    bool connectToDB();
    bool databaseExists(const std::string& dbName);
    bool createDatabase(const std::string& dbName);
    void disconnect();
    bool beginTransaction();
    bool commitTransaction();
    bool rollbackTransaction();
    bool executeQuery(const std::string& query);
    void printResults();
    void showError(SQLSMALLINT handleType, SQLHANDLE handle, const std::string& message);
    static void transactionExample(PostgreSQLConnector& connector);
    bool insertCallSummary(const CallSummary& callSummary);

};
#endif