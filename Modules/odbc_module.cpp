#include "odbc_module.h"
#include <sql.h>
#include <sqlext.h>
#include <iostream>
#include <string>
#include <iomanip>

#include "../settings.h"
#include "../Parsers/call_state_parser.h"

// Конструктор
PostgreSQLConnector::PostgreSQLConnector() : henv(nullptr), hdbc(nullptr), hstmt(nullptr), transactionActive(false), isConnected(false) {}

// Деструктор
PostgreSQLConnector::~PostgreSQLConnector() 
{
    disconnect();
}

bool PostgreSQLConnector::connectToServer()
{
    if (isConnected) 
    {
        disconnect();
    }

    //SQLAllocHandle - allocate memory for env handling
    //SQL_HANDLE_ENV - environment type
    //SQL_NULL_HANDLE - parent handle (currently missing)
    //&henv - pointer to handle variable
    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
    if (!SQL_SUCCEEDED(ret)) 
    {   
        appendLog("Allocating environment handler error\n");
        #ifdef DEBUG
        std::cerr << "Allocating environment handler error" << std::endl;
        return false;
        #endif
        
    }
    
    //SQLSetEnvAttr - setup env agrs
    //SQL_ATTR_ODBC_VERSION - odbc version atribute
    //SQL_OV_ODBC3 - ???
    ret = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (!SQL_SUCCEEDED(ret)) 
    {
        appendLog("Figuring ODBC version error\n");
        #ifdef DEBUG
        std::cerr << "Figuring ODBC version error" << std::endl;
        #endif
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        return false;
    }

    //SQLAllocHandle - creating connection handle
    //SQL_HANDLE_DBC - type
    //henv - parent
    ret = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
    if (!SQL_SUCCEEDED(ret)) 
    {
        appendLog("Allocating connection handler error\n");
        #ifdef DEBUG
        std::cerr << "Allocating connection handler error" << std::endl;
        #endif
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        return false;
    }


    SQLCHAR outConnStr[1024];
    std::string connStr = 
        "DRIVER={PostgreSQL Unicode};"
        "SERVER=" + getSQLhost() + ";"
        "PORT=" + getSQLport() + ";"
        "UID=" + getSQLuser() + ";"
        "PWD=" + getSQLpass() + ";"
        "SSLMODE=prefer;";
    
    ret = SQLDriverConnectA(hdbc, NULL,
                           (SQLCHAR*)connStr.c_str(), SQL_NTS,
                           outConnStr, sizeof(outConnStr), NULL,
                           SQL_DRIVER_NOPROMPT);
    
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_DBC, hdbc, "SQL server connection error");
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        hdbc = nullptr;
        henv = nullptr;
        return false;
    }

    isConnected = true;
    appendLog("Connection to PostgreSQL established succesfully!\n");
    #ifdef DEBUG
    std::cout << "Connection to PostgreSQL established succesfully!" << std::endl;
    #endif
    return true;
}

bool PostgreSQLConnector::connectToDB()
{
    if (isConnected) 
    {
        disconnect();
    }

    //SQLAllocHandle - allocate memory for env handling
    //SQL_HANDLE_ENV - environment type
    //SQL_NULL_HANDLE - parent handle (currently missing)
    //&henv - pointer to handle variable
    ret = SQLAllocHandle(SQL_HANDLE_ENV, SQL_NULL_HANDLE, &henv);
    if (!SQL_SUCCEEDED(ret)) 
    {
        appendLog("Allocating environment handler error\n");
        #ifdef DEBUG
        std::cerr << "Allocating environment handler error" << std::endl;
        #endif
        return false;
    }
    
    //SQLSetEnvAttr - setup env agrs
    //SQL_ATTR_ODBC_VERSION - odbc version atribute
    //SQL_OV_ODBC3 - ???
    ret = SQLSetEnvAttr(henv, SQL_ATTR_ODBC_VERSION, (SQLPOINTER)SQL_OV_ODBC3, 0);
    if (!SQL_SUCCEEDED(ret)) 
    {
        appendLog("Figuring ODBC version error\n");
        #ifdef DEBUG
        std::cerr << "Figuring ODBC version error" << std::endl;
        #endif
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        return false;
    }

    //SQLAllocHandle - creating connection handle
    //SQL_HANDLE_DBC - type
    //henv - parent
    ret = SQLAllocHandle(SQL_HANDLE_DBC, henv, &hdbc);
    if (!SQL_SUCCEEDED(ret)) 
    {
        appendLog("Allocating connection handler error\n");
        #ifdef DEBUG
        std::cerr << "Allocating connection handler error" << std::endl;
        #endif
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        return false;
    }


    SQLCHAR outConnStr[1024];
    std::string connStr = 
        "DRIVER={PostgreSQL Unicode};"
        "SERVER=" + getSQLhost() + ";"
        "PORT=" + getSQLport() + ";"
        "DATABASE=" + getSQLdb() + ";"
        "UID=" + getSQLuser() + ";"
        "PWD=" + getSQLpass() + ";"
        "SSLMODE=prefer;";
    
    ret = SQLDriverConnectA(hdbc, NULL,
                           (SQLCHAR*)connStr.c_str(), SQL_NTS,
                           outConnStr, sizeof(outConnStr), NULL,
                           SQL_DRIVER_NOPROMPT);
    
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_DBC, hdbc, "Connection to DB error");
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        hdbc = nullptr;
        henv = nullptr;
        return false;
    }

    isConnected = true;
    appendLog("Connection to DB established succesfully\n");
    #ifdef DEBUG
    std::cout << "Connection to DB established succesfully" << std::endl;
    #endif
    return true;
}

bool PostgreSQLConnector::databaseExists(const std::string& dbName) 
{
    if (!isConnected) 
    {
        appendLog("Cannot connect to server\n");
        #ifdef DEBUG
        std::cerr << "Cannot connect to server" << std::endl;
        #endif
        return false;
    }

    SQLHSTMT hstmt;
    ret = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_DBC, hdbc, "Cannot allocate statement handle");
        return false;
    }

    std::string query = 
        "SELECT 1 FROM pg_database WHERE datname = '" + dbName + "'";

    ret = SQLExecDirectA(hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    
    bool exists = false;
    if (SQL_SUCCEEDED(ret)) 
    {
        if (SQL_SUCCEEDED(SQLFetch(hstmt))) 
        {
            exists = true;
        }
    }
    else
    {
        showError(SQL_HANDLE_STMT, hstmt, "Error while running query to check for DB existance");
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    return exists;
}

bool PostgreSQLConnector::createDatabase(const std::string& dbName) 
{
    if (!isConnected) 
    {
        appendLog("Cannot connect to server\n");
        #ifdef DEBUG
        std::cerr << "Cannot connect to server" << std::endl;
        #endif
        return false;
    }

    SQLHSTMT hstmt;
    ret = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_DBC, hdbc, "Allocating statement handle erre");
        return false;
    }

    std::string query = "CREATE DATABASE " + dbName + " ENCODING 'UTF8'";
    
    ret = SQLExecDirectA(hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_STMT, hstmt, "Creating DB error");
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        return false;
    }
    SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
    appendLog("Database '" + dbName + "' created succesfully!\n");
    #ifdef DEBUG
    std::cout << "Database '" << dbName << "' created succesfully!" << std::endl;
    #endif

    reconfigureSQLdb(dbName);

    disconnect();
    
    if (!connectToDB())
    {
        appendLog("Failed to connect to new database\n");
        #ifdef DEBUG
        std::cerr << "Failed to connect to new database" << std::endl;
        #endif
        return false;
    }

    SQLHSTMT hstmt_table;
    ret = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt_table);
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_DBC, hdbc, "Allocating statement handle for table creation error");
        return false;
    }

    std::string createTableQuery = R"(
        CREATE TABLE calls
        (
            id SERIAL PRIMARY KEY,
            entry_id VARCHAR(255) NOT NULL,
            call_direction INTEGER NOT NULL,
            from_extension VARCHAR(255),
            from_number VARCHAR(255),
            to_number VARCHAR(255),
            line_number VARCHAR(255),
            
            create_time TIMESTAMP WITH TIME ZONE NOT NULL,
            forward_time TIMESTAMP WITH TIME ZONE,
            talk_time TIMESTAMP WITH TIME ZONE,
            end_time TIMESTAMP WITH TIME ZONE,
            
            entry_result INTEGER,
            disconnect_reason INTEGER,
            sip_call_id VARCHAR(255),
            
            created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
        )
    )";

    ret = SQLExecDirectA(hstmt_table, (SQLCHAR*)createTableQuery.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_STMT, hstmt_table, "Creating table error");
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt_table);
        return false;
    }

    SQLFreeHandle(SQL_HANDLE_STMT, hstmt_table);
    appendLog("Table for storing calls created successfully!\n");
    #ifdef DEBUG
    std::cout << "Table for storing calls created successfully!" << std::endl;
    #endif

    return true;
}

void PostgreSQLConnector::disconnect() 
{
    if (hstmt) 
    {
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        hstmt = nullptr;
    }
    if (hdbc) 
    {
        SQLDisconnect(hdbc);
        SQLFreeHandle(SQL_HANDLE_DBC, hdbc);
        hdbc = nullptr;
    }
    if (henv) 
    {
        SQLFreeHandle(SQL_HANDLE_ENV, henv);
        henv = nullptr;
    }
}

bool PostgreSQLConnector::beginTransaction() 
{
    //turning autocommit off
    ret = SQLSetConnectAttr(hdbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_OFF, 0);
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_DBC, hdbc, "Turning autocommit off error");
        return false;
    }
    transactionActive = true;
    return true;
}

bool PostgreSQLConnector::commitTransaction() 
{
    if (!transactionActive) 
    {
        return false;
    }

    ret = SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_COMMIT);
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_DBC, hdbc, "Commiting transaction error");
        return false;
    }

    //setting autocommit to default
    ret = SQLSetConnectAttr(hdbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_DBC, hdbc, "Turning autocommit on error");
    }
    transactionActive = false;
    return true;
}

bool PostgreSQLConnector::rollbackTransaction() 
{
    if (!transactionActive)
    {
        return false;
    }

    ret = SQLEndTran(SQL_HANDLE_DBC, hdbc, SQL_ROLLBACK);
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_DBC, hdbc, "Transaction rollback error");
        return false;
    }

    ret = SQLSetConnectAttr(hdbc, SQL_ATTR_AUTOCOMMIT, (SQLPOINTER)SQL_AUTOCOMMIT_ON, 0);
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_DBC, hdbc, "Turning autocommit on while rolling back error");
    }

    transactionActive = false;
    return true;
}

bool PostgreSQLConnector::executeQuery(const std::string& query) 
{
    //clearing previous handle
    if (hstmt) 
    {
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        hstmt = nullptr;
    }

    //creating new handle
    ret = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_DBC, hdbc, "Allocating handle whire executng query error");
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        hstmt = nullptr;
        return false;
    }

    //SQLExecDirectA - executes sql statement directly
    ret = SQLExecDirectA(hstmt, (SQLCHAR*)query.c_str(), SQL_NTS);
    if (!SQL_SUCCEEDED(ret)) 
    {
        showError(SQL_HANDLE_STMT, hstmt, "Executing query error");
        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        hstmt = nullptr;
        return false;
    }

    return true;
}

void PostgreSQLConnector::showError(SQLSMALLINT handleType,
               SQLHANDLE handle, //exact handle where error occured
               const std::string& message)
{
    SQLCHAR sqlState[6];         //error code (5+nullptr)
    SQLINTEGER nativeError;      //native code
    SQLCHAR errorMsg[SQL_MAX_MESSAGE_LENGTH];
    SQLSMALLINT errorMsgLen;
    
    SQLRETURN ret = SQLGetDiagRecA(handleType, handle, 1, sqlState, &nativeError,
                                 errorMsg, sizeof(errorMsg), &errorMsgLen);
    
    if (SQL_SUCCEEDED(ret)) 
    {
        appendLog("SQL State: " + std::string((char*)sqlState) + "\n");
        appendLog("Native Error: " + std::to_string(nativeError) + "\n");
        appendLog("Error Message: " + std::string((char*)errorMsg) + "\n");
        #ifdef DEBUG
        std::cerr << "SQL State: " << sqlState << std::endl;
        std::cerr << "Native Error: " << nativeError << std::endl;
        std::cerr << "Error Message: " << errorMsg << std::endl;
        #endif
    }
}

bool PostgreSQLConnector::insertCallSummary(const CallSummary& callSummary)
{
    try
    {
        if (!isConnected) 
        {
            appendLog("Not connected to database\n");
            #ifdef DEBUG
            std::cerr << "Not connected to database" << std::endl;
            #endif
            return false;
        }

        SQLHSTMT hstmt;
        ret = SQLAllocHandle(SQL_HANDLE_STMT, hdbc, &hstmt);
        if (!SQL_SUCCEEDED(ret)) 
        {
            showError(SQL_HANDLE_DBC, hdbc, "Allocating statement handle error");
            return false;
        }

        std::string checkTableQuery = R"(
            SELECT EXISTS (
                SELECT FROM information_schema.tables 
                WHERE table_schema = 'public' 
                AND table_name = 'calls'
            )
        )";

        ret = SQLExecDirectA(hstmt, (SQLCHAR*)checkTableQuery.c_str(), SQL_NTS);
        if (!SQL_SUCCEEDED(ret)) 
        {
            showError(SQL_HANDLE_STMT, hstmt, "No such table exists, reconfigure required");
            SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
            return false;
        }

        bool tableExists = false;
        SQLFreeStmt(hstmt, SQL_CLOSE);

        std::string insertQuery = R"(
            INSERT INTO calls (
                entry_id, call_direction, from_extension, from_number, 
                to_number, line_number, create_time, forward_time, 
                talk_time, end_time, entry_result, disconnect_reason, sip_call_id
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        )";

        ret = SQLPrepareA(hstmt, (SQLCHAR*)insertQuery.c_str(), SQL_NTS);
        if (!SQL_SUCCEEDED(ret)) 
        {
            showError(SQL_HANDLE_STMT, hstmt, "Preparing insert statement error");
            SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
            return false;
        }

        SQLBindParameter(hstmt, 1, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
                        255, 0, (SQLCHAR*)callSummary.entry_id.c_str(), 
                        callSummary.entry_id.length(), NULL);
        
        SQLBindParameter(hstmt, 2, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 
                        0, 0, (SQLPOINTER)&callSummary.call_direction, 0, NULL);
        
        SQLBindParameter(hstmt, 3, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
                        255, 0, (SQLCHAR*)callSummary.from_extension.c_str(), 
                        callSummary.from_extension.length(), NULL);
        
        SQLBindParameter(hstmt, 4, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
                        255, 0, (SQLCHAR*)callSummary.from_number.c_str(), 
                        callSummary.from_number.length(), NULL);
        
        SQLBindParameter(hstmt, 5, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
                        255, 0, (SQLCHAR*)callSummary.to_number.c_str(), 
                        callSummary.to_number.length(), NULL);
        
        SQLBindParameter(hstmt, 6, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
                        255, 0, (SQLCHAR*)callSummary.line_number.c_str(), 
                        callSummary.line_number.length(), NULL);

        
        SQLBindParameter(hstmt, 7, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
                        callSummary.create_time.size(), 0,
                        (SQLPOINTER)callSummary.create_time.c_str(),
                        callSummary.create_time.size(), NULL);

        SQLBindParameter(hstmt, 8, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
                        callSummary.forward_time.size(), 0,
                        (SQLPOINTER)callSummary.forward_time.c_str(),
                        callSummary.forward_time.size(), NULL);

        SQLBindParameter(hstmt, 9, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
                        callSummary.talk_time.size(), 0,
                        (SQLPOINTER)callSummary.talk_time.c_str(),
                        callSummary.talk_time.size(), NULL);

        SQLBindParameter(hstmt, 10, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
                        callSummary.end_time.size(), 0,
                        (SQLPOINTER)callSummary.end_time.c_str(),
                        callSummary.end_time.size(), NULL);
                        
        
        SQLBindParameter(hstmt, 11, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 
                        0, 0, (SQLPOINTER)&callSummary.entry_result, 0, NULL);
        
        SQLBindParameter(hstmt, 12, SQL_PARAM_INPUT, SQL_C_LONG, SQL_INTEGER, 
                        0, 0, (SQLPOINTER)&callSummary.disconnect_reason, 0, NULL);
        
        SQLBindParameter(hstmt, 13, SQL_PARAM_INPUT, SQL_C_CHAR, SQL_VARCHAR, 
                        255, 0, (SQLCHAR*)callSummary.sip_call_id.c_str(), 
                        callSummary.sip_call_id.length(), NULL);

        if (!beginTransaction()) 
        {
            throw std::runtime_error("Cannot begin transaction");
        }
        
        ret = SQLExecute(hstmt);
        if (!SQL_SUCCEEDED(ret)) 
        {
            showError(SQL_HANDLE_STMT, hstmt, "Executing insert statement error");
            SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
            rollbackTransaction();
            throw std::runtime_error("Error while executing query");
            return false;
        }
        if (!commitTransaction()) 
        {
            throw std::runtime_error("Transaction commit error");
        }

        SQLFreeHandle(SQL_HANDLE_STMT, hstmt);
        appendLog("Call summary inserted successfully!\n");
        #ifdef DEBUG
        std::cout << "Call summary inserted successfully!" << std::endl;
        #endif
        return true;
    }
    catch (const std::exception& e) 
    {
        appendLog("Transaction error: " + std::string(e.what()) + "\n");
        #ifdef DEBUG
        std::cerr << "Transaction error: " << e.what() << std::endl;
        #endif
        return false;
    }
}