/*******************************************************************************
 * libretroshare/src/util: retrodb.cc                                          *
 *                                                                             *
 * libretroshare: retroshare core library                                      *
 *                                                                             *
 * Copyright (C) 2012  Christopher Evi-Parker <retroshare@lunamutt.com>        *
 * Copyright (C) 2019  Gioacchino Mazzurco <gio@altermundi.net>                *
 *                                                                             *
 * This program is free software: you can redistribute it and/or modify        *
 * it under the terms of the GNU Lesser General Public License as              *
 * published by the Free Software Foundation, either version 3 of the          *
 * License, or (at your option) any later version.                             *
 *                                                                             *
 * This program is distributed in the hope that it will be useful,             *
 * but WITHOUT ANY WARRANTY; without even the implied warranty of              *
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                *
 * GNU Lesser General Public License for more details.                         *
 *                                                                             *
 * You should have received a copy of the GNU Lesser General Public License    *
 * along with this program. If not, see <https://www.gnu.org/licenses/>.       *
 *                                                                             *
 *******************************************************************************/

#include <iostream>
#include <sstream>
#include <memory>
#include <cstdint>
#include <cerrno>

#include "util/rstime.h"
#include "util/retrodb.h"
#include "util/rsdbbind.h"
#include "util/stacktrace.h"
#include "util/rsdir.h"

//#define RETRODB_DEBUG

const int RetroDb::OPEN_READONLY = SQLITE_OPEN_READONLY;
const int RetroDb::OPEN_READWRITE = SQLITE_OPEN_READWRITE;
const int RetroDb::OPEN_READWRITE_CREATE = SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE;

RetroDb::RetroDb(const std::string& dbPath, int flags, const std::string& key):
    mDb(nullptr), mKey(key),mDbNeedsCleaning(false),mPath(dbPath)
{
	bool alreadyExists = RsDirUtil::fileExists(dbPath);

	int rc = sqlite3_open_v2(dbPath.c_str(), &mDb, flags, nullptr);
	if(rc)
	{
		RsErr() << __PRETTY_FUNCTION__ << " Can't open database, Error: "
		        << rc << " " <<  sqlite3_errmsg(mDb) << std::endl;
		closeDb();
		print_stacktrace();
		return;
	}

	if(alreadyExists)
	{
		/* If the database has been created by a RetroShare compiled without
		 * SQLCipher, open it as a plain SQLite database instead of failing
		 * miserably. If RetroShare has been compiled without SQLCipher but the
		 * database seems encrypted print a meaningful error message instead of
		 * crashing miserably.
		 * At some point we could implement a migration SQLite <-> SQLCipher
		 * mecanism and suggest it to the user, or give the option to the user
		 * to choice between plain SQLite or SQLCipher database, is some cases
		 * such as encrypted FS it might make sense to keep SQLite even if
		 * SQLCipher is availble for performance, as encryption is already
		 * provided at FS level. */

		rc = sqlite3_exec( mDb, "PRAGMA schema_version;",
		                   nullptr, nullptr, nullptr );
		if( rc == SQLITE_OK )
		{
#ifndef NO_SQLCIPHER
			RsWarn() << __PRETTY_FUNCTION__ << " The database is not encrypted: "
			         << dbPath << std::endl;
#endif // ndef NO_SQLCIPHER

			return;
		}
		else
		{
#ifdef NO_SQLCIPHER
			RsErr() << __PRETTY_FUNCTION__ << " Error quering schema version."
			        << " Are you trying to open an encrypted database without "
			        << "compiling SQLCipher support?" << std::endl << std::endl;
			print_stacktrace();
			closeDb();
#else // def NO_SQLCIPHER
			RsInfo() << __PRETTY_FUNCTION__ << " The database seems encrypted: "
			         << dbPath << std::endl;
#endif // def NO_SQLCIPHER
		}
	}

#ifndef NO_SQLCIPHER
	if(!mKey.empty())
	{
		rc = sqlite3_key(mDb, mKey.c_str(), static_cast<int>(mKey.size()));

		if(rc)
		{
			RsErr() << __PRETTY_FUNCTION__ << " Can't key database: " << rc
			        << " " <<  sqlite3_errmsg(mDb) << std::endl;
			closeDb();
			return;
		}
	}

	char* err = nullptr;
	rc = sqlite3_exec(mDb, "PRAGMA cipher_migrate;", nullptr, nullptr, &err);
	if (rc != SQLITE_OK)
	{
		RsErr() << __PRETTY_FUNCTION__ << " Error upgrading database, error "
		        << "code: " << rc << " " << err << std::endl;
		sqlite3_free(err);
	}

	// Test DB for correct sqlcipher version
	if(sqlite3_exec(
	            mDb, "PRAGMA user_version;",
	            nullptr, nullptr, nullptr ) != SQLITE_OK)
	{
		RsWarn() << __PRETTY_FUNCTION__ << " Failed to open database: "
		         << dbPath << std::endl;

		//Reopening the database with correct settings
		closeDb();
		if(!rc) rc = sqlite3_open_v2(dbPath.c_str(), &mDb, flags, nullptr);
		if(!rc && !mKey.empty())
			rc = sqlite3_key(mDb, mKey.c_str(), static_cast<int>(mKey.size()));
		if(!rc)
			rc = sqlite3_exec( mDb, "PRAGMA kdf_iter = 64000;",
			                   nullptr, nullptr, nullptr );
		if (!rc && (sqlite3_exec( mDb, "PRAGMA user_version;",
		                          nullptr, nullptr, nullptr ) == SQLITE_OK))
		{
			RsInfo() << __PRETTY_FUNCTION__ << " Re-trying with settings for "
			         << "sqlcipher version 3 successed" << std::endl;
		}
		else
		{
			RsErr() << __PRETTY_FUNCTION__ << " Re-trying with settings for "
			        << "sqlcipher version 3 failed, giving up" << std::endl;
			closeDb();
			return;
		}
	}
#endif // ndef NO_SQLCIPHER
}

RetroDb::~RetroDb() { closeDb(); }

void RetroDb::closeDb()
{
    if(!mDb)
        return;

    if(mDbNeedsCleaning)
    {
        RsDbg() << "Cleaning the Db \"" << mPath << "\" using the VACUUM command." ;
        execSQL("VACUUM;");
        mDbNeedsCleaning = false;
    }

	// no-op if mDb is nullptr (https://www.sqlite.org/c3ref/close.html)
	int rc = sqlite3_close(mDb);
	mDb = nullptr;

	Dbg2() << __PRETTY_FUNCTION__ << " sqlite3_close return: " << rc
	       << std::endl;
}

#define TIME_LIMIT 3

bool RetroDb::execSQL(const std::string &query){

    // prepare statement
    sqlite3_stmt* stm = NULL;

#ifdef RETRODB_DEBUG
    std::cerr << "Query: " << query << std::endl;
#endif

    int rc = sqlite3_prepare_v2(mDb, query.c_str(), query.length(), &stm, NULL);

    // check if there are any errors
    if(rc != SQLITE_OK){
        std::cerr << "RetroDb::execSQL(): Error preparing statement\n";
        std::cerr << "Error code: " <<  sqlite3_errmsg(mDb)
                  << std::endl;
        return false;
    }


    rstime_t stamp = time(NULL);
    bool timeOut = false, ok = false;

    while(!timeOut){

        rc = sqlite3_step(stm);

        if(rc == SQLITE_DONE){
            ok = true;
            break;
        }

        if(rc != SQLITE_BUSY){
            ok = false;
            break;
        }

        if(time(NULL) > stamp + TIME_LIMIT)
        {
            ok = false;
            timeOut = true;
        }
        // TODO add sleep so not to waste
        // precious cycles
    }

    if(!ok){

        if(rc == SQLITE_BUSY){
            std::cerr << "RetroDb::execSQL()\n" ;
            std::cerr << "SQL timed out!" << std::endl;
        }else{
            std::cerr << "RetroDb::execSQL(): Error executing statement (code: " << rc << ")\n";
            std::cerr << "Sqlite Error msg: " <<  sqlite3_errmsg(mDb)
                      << std::endl;
            std::cerr << "RetroDb::execSQL() Query: " <<  query << std::endl;
        }
    }

    // finalise statement or else db cannot be closed
    sqlite3_finalize(stm);
    return ok;
}

RetroCursor* RetroDb::sqlQuery(const std::string& tableName, const std::list<std::string>& columns,
                               const std::string& selection, const std::string& orderBy){

    if(tableName.empty() || columns.empty()){
        std::cerr << "RetroDb::sqlQuery(): No table or columns given" << std::endl;
        return NULL;
    }

    std::string columnSelection; // the column names to return
    sqlite3_stmt* stmt = NULL;
    std::list<std::string>::const_iterator it = columns.begin();

    for(; it != columns.end(); ++it){
        if (it != columns.begin()) {
            columnSelection += ",";
        }

        columnSelection += *it;
    }

    // construct query
    // SELECT columnSelection FROM tableName WHERE selection
    std::string sqlQuery = "SELECT " + columnSelection + " FROM " +
                           tableName;

    // add selection clause if present
    if(!selection.empty())
        sqlQuery += " WHERE " + selection;


    // add 'order by' clause if present
    if(!orderBy.empty())
        sqlQuery += " ORDER BY " + orderBy + ";";
    else
        sqlQuery += ";";

#ifdef RETRODB_DEBUG
    std::cerr << "RetroDb::sqlQuery(): " << sqlQuery << std::endl;
#endif

    sqlite3_prepare_v2(mDb, sqlQuery.c_str(), sqlQuery.length(), &stmt, NULL);
    return (new RetroCursor(stmt));
}

bool RetroDb::isOpen() const {
    return (mDb==NULL ? false : true);
}

bool RetroDb::sqlInsert(const std::string &table, const std::string& /* nullColumnHack */, const ContentValue &cv){

    std::map<std::string, uint8_t> keyTypeMap;
    cv.getKeyTypeMap(keyTypeMap);
    std::map<std::string, uint8_t>::iterator mit = keyTypeMap.begin();

    // build columns part of insertion
    std::string qColumns = table + "(";

    for(; mit != keyTypeMap.end(); ++mit){

        qColumns += mit->first;

        ++mit;

        // add comma if more columns left
        if(mit == keyTypeMap.end())
            qColumns += ")"; // close bracket if at end
        else
            qColumns += ",";

        --mit;
    }

    // build values part of insertion
    std::string qValues;
    std::list<RetroBind*> paramBindings;
    buildInsertQueryValue(keyTypeMap, cv, qValues, paramBindings);

    // complete insertion query
    std::string sqlQuery = "INSERT INTO " + qColumns + " " + qValues;

    bool ok = execSQL_bind(sqlQuery, paramBindings);

#ifdef RETRODB_DEBUG
    std::cerr << "RetroDb::sqlInsert(): " << sqlQuery << std::endl;
#endif

    return ok;
}

std::string RetroDb::getKey() const
{
	return mKey;
}

bool RetroDb::beginTransaction()
{
    if (!isOpen()) {
        return false;
    }

    return execSQL("BEGIN;");
}

bool RetroDb::commitTransaction()
{
    if (!isOpen()) {
        return false;
    }

    return execSQL("COMMIT;");
}
bool RetroDb::rollbackTransaction()
{
    if (!isOpen()) {
        return false;
    }

    return execSQL("ROLLBACK;");
}

bool RetroDb::execSQL_bind(const std::string &query, std::list<RetroBind*> &paramBindings){

    // prepare statement
    sqlite3_stmt* stm = NULL;

#ifdef RETRODB_DEBUG
    std::cerr << "Query: " << query << std::endl;
#endif

    int rc = sqlite3_prepare_v2(mDb, query.c_str(), query.length(), &stm, NULL);

    // check if there are any errors
    if(rc != SQLITE_OK){
        std::cerr << "RetroDb::execSQL_bind(): Error preparing statement\n";
        std::cerr << "Error code: " <<  sqlite3_errmsg(mDb)
                  << std::endl;
        return false;
    }

    std::list<RetroBind*>::iterator lit = paramBindings.begin();

    for(; lit != paramBindings.end(); ++lit){
        RetroBind* rb = *lit;

        if(!rb->bind(stm))
        {
        	std::cerr << "\nBind failed for index: " << rb->getIndex()
        			  << std::endl;
        }

        delete rb;
        rb = NULL;
    }

    uint32_t delta = 3;
    rstime_t stamp = time(NULL), now = 0;
    bool timeOut = false, ok = false;

    while(!timeOut){

        rc = sqlite3_step(stm);

        if(rc == SQLITE_DONE){
            ok = true;
            break;
        }

        if(rc != SQLITE_BUSY){
            ok = false;
            break;
        }

        now = time(NULL);
        delta = stamp - now;

        if(delta > TIME_LIMIT){
            ok = false;
            timeOut = true;
        }
        // TODO add sleep so not to waste
        // precious cycles
    }

    if(!ok){

        if(rc == SQLITE_BUSY){
            std::cerr << "RetroDb::execSQL_bind()\n" ;
            std::cerr << "SQL timed out!" << std::endl;
        }else{
            std::cerr << "RetroDb::execSQL_bind(): Error executing statement (code: " << rc << ")\n";
            std::cerr << "Sqlite Error msg: " <<  sqlite3_errmsg(mDb)
                      << std::endl;
            std::cerr << "RetroDb::execSQL_bind() Query: " <<  query << std::endl;
        }
    }

    // finalise statement or else db cannot be closed
    sqlite3_finalize(stm);
    return ok;
}

void RetroDb::buildInsertQueryValue(const std::map<std::string, uint8_t> keyTypeMap,
		const ContentValue& cv, std::string& parameter,
		std::list<RetroBind*>& paramBindings)
{
	std::map<std::string, uint8_t>::const_iterator mit = keyTypeMap.begin();

	parameter = "VALUES(";
	int index = 0;
    for(mit=keyTypeMap.begin(); mit!=keyTypeMap.end(); ++mit)
    {

        uint8_t type = mit->second;
        std::string key = mit->first;

        RetroBind* rb = NULL;
        if(ContentValue::BOOL_TYPE == type)
            {
                bool value;
                cv.getAsBool(key, value);
                rb = new RsBoolBind(value, ++index);
            }
        else if( ContentValue::DOUBLE_TYPE == type)
            {
                double value;
                cv.getAsDouble(key, value);
                rb = new RsDoubleBind(value, ++index);
            }
        else if( ContentValue::DATA_TYPE == type)
            {
                char* value;
                uint32_t len;
                cv.getAsData(key, len, value);
                rb = new RsBlobBind(value, len, ++index);
            }
        else if ( ContentValue::STRING_TYPE == type)
            {
                std::string value;
                cv.getAsString(key, value);
                rb = new RsStringBind(value, ++index);
            }
        else if ( ContentValue::INT32_TYPE == type)
            {
                int32_t value = 0;
                cv.getAsInt32(key, value);
                rb = new RsInt32Bind(value, ++index);
            }
        else if( ContentValue::INT64_TYPE == type)
            {
                int64_t value = 0;
                cv.getAsInt64(key, value);
                rb = new RsInt64bind(value, ++index);
            }

        if(rb)
        {
        	paramBindings.push_back(rb);

        	++mit;

        	if(mit == keyTypeMap.end())
        		parameter += "?";
        	else
        		parameter += "?,";

        	--mit;
        }
    }

    parameter += ")";


}

void RetroDb::buildUpdateQueryValue(const std::map<std::string, uint8_t> keyTypeMap,
		const ContentValue& cv, std::string& parameter,
		std::list<RetroBind*>& paramBindings)
{
	std::map<std::string, uint8_t>::const_iterator mit = keyTypeMap.begin();

	int index = 0;
    for(mit=keyTypeMap.begin(); mit!=keyTypeMap.end(); ++mit)
    {

        uint8_t type = mit->second;
        std::string key = mit->first;

        RetroBind* rb = NULL;
        if(ContentValue::BOOL_TYPE == type)
            {
                bool value;
                cv.getAsBool(key, value);
                rb = new RsBoolBind(value, ++index);
            }
        else if( ContentValue::DOUBLE_TYPE == type)
            {
                double value;
                cv.getAsDouble(key, value);
                rb = new RsDoubleBind(value, ++index);
            }
        else if( ContentValue::DATA_TYPE == type)
            {
                char* value;
                uint32_t len;
                cv.getAsData(key, len, value);
                rb = new RsBlobBind(value, len, ++index);
            }
        else if ( ContentValue::STRING_TYPE == type)
            {
                std::string value;
                cv.getAsString(key, value);
                rb = new RsStringBind(value, ++index);
            }
        else if ( ContentValue::INT32_TYPE == type)
            {
                int32_t value = 0;
                cv.getAsInt32(key, value);
                rb = new RsInt32Bind(value, ++index);
            }
        else if( ContentValue::INT64_TYPE == type)
            {
                int64_t value = 0;
                cv.getAsInt64(key, value);
                rb = new RsInt64bind(value, ++index);
            }

        if(rb)
        {
        	paramBindings.push_back(rb);

        	++mit;

        	if(mit == keyTypeMap.end())
        		parameter += key + "=?";
        	else
        		parameter += key + "=?,";

        	--mit;
        }
    }

}

bool RetroDb::sqlDelete(const std::string &tableName, const std::string &whereClause, const std::string &/*whereArgs*/){

    std::string sqlQuery = "DELETE FROM " + tableName;

    if(!whereClause.empty()){
        sqlQuery += " WHERE " + whereClause + ";";
    }else
        sqlQuery += ";";

    bool res = execSQL(sqlQuery);

    if(res)
    {
        RsDbg() << "After deletion from Db \"" << mPath << "\", a cleaning operation will occur when closing." ;
        mDbNeedsCleaning = true;
    }

    return res;
}


bool RetroDb::sqlUpdate(const std::string &tableName, std::string whereClause, const ContentValue& cv){

    std::string sqlQuery = "UPDATE " + tableName + " SET ";


    std::map<std::string, uint8_t> keyTypeMap;
    std::map<std::string, uint8_t>::iterator mit;
    cv.getKeyTypeMap(keyTypeMap);

    // build SET part of update
    std::string qValues;
    std::list<RetroBind*> paramBindings;
    buildUpdateQueryValue(keyTypeMap, cv, qValues, paramBindings);

    if(qValues.empty())
        return false;
    else
        sqlQuery += qValues;

    // complete update
    if(!whereClause.empty()){
        sqlQuery += " WHERE " +  whereClause + ";";
    }
    else{
        sqlQuery += ";";
    }

    // execute query
    return execSQL_bind(sqlQuery, paramBindings);
}

bool RetroDb::tableExists(const std::string &tableName)
{
    if (!isOpen()) {
        return false;
    }

    std::string sqlQuery = "PRAGMA table_info(" + tableName + ");";

    bool result = false;
    sqlite3_stmt* stmt = NULL;

    int rc = sqlite3_prepare_v2(mDb, sqlQuery.c_str(), sqlQuery.length(), &stmt, NULL);
    if (rc == SQLITE_OK) {
        rc = sqlite3_step(stmt);
        switch (rc) {
        case SQLITE_ROW:
            result = true;
            break;
        case SQLITE_DONE:
            break;
        default:
            std::cerr << "RetroDb::tableExists(): Error executing statement (code: " << rc << ")"
                      << std::endl;
            return false;
        }
    } else {
        std::cerr << "RetroDb::tableExists(): Error preparing statement\n";
        std::cerr << "Error code: " <<  sqlite3_errmsg(mDb)
                  << std::endl;
    }

    if (stmt) {
        sqlite3_finalize(stmt);
    }

    return result;
}

/********************** RetroCursor ************************/

RetroCursor::RetroCursor(sqlite3_stmt *stmt)
    : mStmt(NULL) {

     open(stmt);
}

RetroCursor::~RetroCursor(){

    // finalise statement
    if(mStmt){
        sqlite3_finalize(mStmt);
    }
}

bool RetroCursor::moveToFirst(){

#ifdef RETRODB_DEBUG
    std::cerr << "RetroCursor::moveToFirst()\n";
#endif

    if(!isOpen())
        return false;

    // reset statement
    int rc = sqlite3_reset(mStmt);

    if(rc != SQLITE_OK){

#ifdef RETRODB_DEBUG
    std::cerr << "Error code: " << rc << std::endl;
#endif

        return false;
    }

    rc = sqlite3_step(mStmt);

    if(rc == SQLITE_ROW){
        return true;
    }

#ifdef RETRODB_DEBUG
    std::cerr << "Error code: " << rc << std::endl;
#endif

    return false;
}

bool RetroCursor::moveToLast(){

#ifdef RETRODB_DEBUG
    std::cerr << "RetroCursor::moveToLast()\n";
#endif

    if(!isOpen())
        return -1;

    // go to begining
    int rc = sqlite3_reset(mStmt);

    if(rc != SQLITE_OK)
        return false;

    rc = sqlite3_step(mStmt);

    while(rc == SQLITE_ROW){
        rc = sqlite3_step(mStmt);
    }

    if(rc != SQLITE_DONE){
        std::cerr << "Error executing statement (code: " << rc << ")\n"
                  << std::endl;
        return false;

    }else{
        return true;
    }
}

int RetroCursor::columnCount() const {

    if(isOpen())
        return sqlite3_data_count(mStmt);
    else
        return -1;
}

bool RetroCursor::isOpen() const {
    return !(mStmt == NULL);
}

bool RetroCursor::close(){

    if(!isOpen())
        return false;


    int rc = sqlite3_finalize(mStmt);
    mStmt = NULL;

    return (rc == SQLITE_OK);
}

bool RetroCursor::open(sqlite3_stmt *stm){

#ifdef RETRODB_DEBUG
    std::cerr << "RetroCursor::open() \n";
#endif

    if(isOpen())
        close();

    mStmt = stm;

    // ensure statement is valid
    int rc = sqlite3_reset(mStmt);

    if(rc == SQLITE_OK){
        return true;
    }
    else{
        std::cerr << "Error Opening cursor (code: " << rc << ")\n";
        close();
        return false;
    }

}

bool RetroCursor::moveToNext(){

#ifdef RETRODB_DEBUG
    std::cerr << "RetroCursor::moveToNext()\n";
#endif

    if(!isOpen())
        return false;

    int rc = sqlite3_step(mStmt);

    if(rc == SQLITE_ROW){
        return true;

    }else if(rc == SQLITE_DONE){ // no more results
        return false;
    }
    else if(rc == SQLITE_BUSY){ // should not enter here
        std::cerr << "RetroDb::moveToNext()\n" ;
        std::cerr << "Busy!, possible multiple accesses to Db" << std::endl
                  << "serious error";

        return false;

    }else{
        std::cerr << "Error executing statement (code: " << rc << ")\n";
        return false;
    }
}

int32_t RetroCursor::getInt32(int columnIndex){
    return sqlite3_column_int(mStmt, columnIndex);
}

int64_t RetroCursor::getInt64(int columnIndex){
    return sqlite3_column_int64(mStmt, columnIndex);
}

bool RetroCursor::getBool(int columnIndex){
    return sqlite3_column_int(mStmt, columnIndex);
}

double RetroCursor::getDouble(int columnIndex){
    return sqlite3_column_double(mStmt, columnIndex);
}

void RetroCursor::getString(int columnIndex, std::string &str){
    str.clear();
    char* raw_str = (char*)sqlite3_column_text(mStmt, columnIndex);
    if(raw_str != NULL)
    {
        str.assign(raw_str);
#ifdef RADIX_STRING
        		char* buffer = NULL;
        		size_t buffLen;
                Radix64::decode(str, buffer, buffLen);
                str.clear();
                if(buffLen != 0)
                {
                	str.assign(buffer, buffLen);
                	delete[] buffer;
                }
                else
                	str.clear();
#endif
    }
    else
    {
       str.clear();
    }
}

const void* RetroCursor::getData(int columnIndex, uint32_t &datSize){

    const void* val = sqlite3_column_blob(mStmt, columnIndex);
    datSize = sqlite3_column_bytes(mStmt, columnIndex);

    return val;
}

