/*  Copyright (C) 2015, Embecosm Limited

    This file is part of MAGEEC

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */

//===--------------------------- MAGEEC database --------------------------===//
//
// This file defines the main interface to the MAGEEC database. This includes
// the interfaces for feature extraction, compilation and access to the
// machine learners. It also provides methods to query an existing database,
// as well as create a new one.
//
//===----------------------------------------------------------------------===//

#include "mageec/Database.h"
#include "mageec/ML.h"
#include "mageec/SQLQuery.h"
#include "mageec/TrainedML.h"
#include "mageec/Types.h"
#include "mageec/Util.h"

#include "sqlite3.h"

#include <cassert>
#include <cstdio>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace mageec {

// Init the version of the database
const util::Version Database::version(MAGEEC_DATABASE_VERSION_MAJOR,
                                      MAGEEC_DATABASE_VERSION_MINOR,
                                      MAGEEC_DATABASE_VERSION_PATCH);

//===------------------- Database table creation queries ------------------===//

// metadata table creation
static const char *const create_metadata_table =
    "CREATE TABLE Metadata(field INTEGER PRIMARY KEY, value TEXT NOT NULL)";

// database feature table creation strings
static const char *const create_feature_type_table =
    "CREATE TABLE FeatureType("
    "feature_id   INTEGER PRIMARY KEY, "
    "feature_type INTEGER NOT NULL"
    ")";

static const char *const create_feature_set_feature_table =
    "CREATE TABLE FeatureSetFeature("
    "feature_set_id INTEGER NOT NULL, "
    "feature_id     INTEGER NOT NULL, "
    "value          BLOB NOT NULL, "
    "UNIQUE(feature_set_id, feature_id), "
    "FOREIGN KEY(feature_id) REFERENCES FeatureType(feature_id)"
    ")";

// database parameter table creation strings
static const char *const create_parameter_type_table =
    "CREATE TABLE ParameterType("
    "parameter_id   INTEGER PRIMARY KEY, "
    "parameter_type INTEGER NOT NULL"
    ")";

static const char *const create_parameter_set_parameter_table =
    "CREATE TABLE ParameterSetParameter("
    "parameter_set_id INTEGER NOT NULL, "
    "parameter_id     INTEGER NOT NULL, "
    "value            BLOB NOT NULL, "
    "UNIQUE(parameter_set_id, parameter_id), "
    "FOREIGN KEY(parameter_id) REFERENCES ParameterType(parameter_id)"
    ")";

// compilation table creation strings
static const char *const create_compilation_table =
    "CREATE TABLE Compilation("
    "compilation_id    INTEGER PRIMARY KEY, "
    "feature_set_id    INTEGER NOT NULL, " 
    "feature_class_id  INTEGER NOT NULL, "
    "parameter_set_id  INTEGER"
    ")";

// result table creation strings
static const char *const create_result_table =
    "CREATE TABLE Result("
    "compilation_id INTEGER NOT NULL, "
    "metric         TEXT NOT NULL, "
    "result         REAL NOT NULL, "
    "UNIQUE(compilation_id, metric), "
    "FOREIGN KEY(compilation_id) REFERENCES Compilation(compilation_id)"
    ")";

// machine learner table creation strings
static const char *const create_machine_learner_table =
    "CREATE TABLE MachineLearner("
    "ml_id             TEXT, "
    "feature_class_id  INTEGER NOT NULL, "
    "metric            TEXT, "
    "ml_blob           BLOB NOT NULL, "
    "UNIQUE(ml_id, metric, feature_class_id)"
    ")";

// debug table creation
static const char *const create_compilation_debug_table =
    "CREATE TABLE CompilationDebug("
    "compilation_id INTEGER PRIMARY KEY, "
    "name           TEXT NOT NULL, "
    "type           TEXT NOT NULL, "
    "command        TEXT, "
    "parent_id      INTEGER, "
    "FOREIGN KEY(compilation_id) "
        "REFERENCES Compilation(compilation_id) ON DELETE CASCADE, "
    "FOREIGN KEY(parent_id) "
        "REFERENCES Compilation(compilation_id) ON DELETE SET NULL"
    ")";

static const char *const create_feature_debug_table =
    "CREATE TABLE FeatureDebug("
    "feature_id INTEGER PRIMARY KEY, "
    "name       TEXT NOT NULL, "
    "FOREIGN KEY(feature_id) REFERENCES FeatureType(feature_id)"
    ")";

static const char *const create_parameter_debug_table =
    "CREATE TABLE ParameterDebug("
    "parameter_id INTEGER PRIMARY KEY, "
    "name              TEXT NOT NULL, "
    "FOREIGN KEY(parameter_id) REFERENCES ParameterType(parameter_id)"
    ")";

//===-------------------- Database implementation -------------------------===//

std::unique_ptr<Database>
Database::createDatabase(std::string db_path,
                         std::map<std::string, IMachineLearner *> mls) {
  // Fail if the file already exists
  std::ifstream f(db_path.c_str());
  if (f.good()) {
    return nullptr;
  }
  f.close();

  sqlite3 *db;
  int res = sqlite3_open(db_path.c_str(), &db);
  if (db == nullptr) {
    return nullptr;
  }
  if (res != SQLITE_OK) {
    sqlite3_close(db);
    return nullptr;
  }
  return std::unique_ptr<Database>(new Database(*db, mls, true));
}

std::unique_ptr<Database>
Database::loadDatabase(std::string db_path,
                       std::map<std::string, IMachineLearner *> mls) {
  // Fail if the file does not already exist
  std::ifstream f(db_path.c_str());
  if (!f.good()) {
    return nullptr;
  }
  f.close();

  sqlite3 *db;
  int res = sqlite3_open(db_path.c_str(), &db);
  if (db == nullptr) {
    return nullptr;
  }
  if (res != SQLITE_OK) {
    sqlite3_close(db);
    return nullptr;
  }
  return std::unique_ptr<Database>(new Database(*db, mls, false));
}

std::unique_ptr<Database>
Database::getDatabase(std::string db_path,
                      std::map<std::string, IMachineLearner *> mls) {
  // First try and load the database, if that fails try and create it
  std::unique_ptr<Database> db;
  MAGEEC_DEBUG("Loading database '" << db_path << "'");
  db = loadDatabase(db_path, mls);
  if (db) {
    MAGEEC_DEBUG("Database '" << db_path << "' loaded");
    return db;
  }
  MAGEEC_DEBUG("Cannot load database, creating new database...");
  db = createDatabase(db_path, mls);
  MAGEEC_DEBUG("Database '" << db_path << "'created");
  return db;
}

Database::Database(sqlite3 &db, std::map<std::string, IMachineLearner *> mls,
                   bool create)
    : m_db(&db), m_mls(mls) {
  // Set a busy timeout for all database transactions of 3 hours
  sqlite3_busy_timeout(m_db, 10000000);

  // Enable foreign keys (requires sqlite 3.6.19 or above)
  // If foreign keys are not available the database is still usable, but no
  // foreign key checking will do done.
  SQLQuery(*m_db, "PRAGMA foreign_keys = ON").exec().assertDone();

  // The MEMORY journaling mode stores the rollback journal in volatile RAM.
  // This saves disk I/O but at the expense of database safety and integrity.
  // If the application using SQLite crashes in the middle of a transaction
  // when the MEMORY journaling mode is set, then the database file will very
  // likely go corrupt.
  //
  // For now we use a memory rollback journal, as it improves performance when
  // we have lots of small transactions and short-lived journals. This WILL
  // corrupt the database if we crash mid transaction. If this proves to
  // be a problem in future, then we may want to move to something like
  // PERSIST instead, which is higher performance that the default (DELETE),
  // and also preserves safety.
  SQLQuery(*m_db, "PRAGMA journal_mode = MEMORY").exec().next().assertDone();

  if (create) {
    init_db(*m_db);
    validate();
  } else {
    if (!isCompatible()) {
      // TODO: trigger exception
      assert(0 && "Loaded incompatible database");
    }
    validate();
  }
}

Database::~Database(void) {
  int res = sqlite3_close(m_db);
  if (res != SQLITE_OK) {
    MAGEEC_DEBUG("Unable to close mageec database:\n" << sqlite3_errmsg(m_db));
  }
  assert(res == SQLITE_OK && "Unable to close mageec database!");
}

void Database::init_db(sqlite3 &db) {
  // Create the entire database in a single transaction
  SQLTransaction transaction(&db);
  MAGEEC_DEBUG("Creating database tables");

  // Create table to hold database metadata
  SQLQuery(db, create_metadata_table).exec().assertDone();

  // Create tables to hold features
  SQLQuery(db, create_feature_type_table).exec().assertDone();
  SQLQuery(db, create_feature_set_feature_table).exec().assertDone();

  // Tables to hold parameters
  SQLQuery(db, create_parameter_type_table).exec().assertDone();
  SQLQuery(db, create_parameter_set_parameter_table).exec().assertDone();

  // Compilation
  SQLQuery(db, create_compilation_table).exec().assertDone();

  // Results
  SQLQuery(db, create_result_table).exec().assertDone();

  // Machine learner
  SQLQuery(db, create_machine_learner_table).exec().assertDone();

  // Debug tables
  SQLQuery(db, create_compilation_debug_table).exec().assertDone();
  SQLQuery(db, create_feature_debug_table).exec().assertDone();
  SQLQuery(db, create_parameter_debug_table).exec().assertDone();

  // Manually insert the version into the metadata table
  SQLQuery query =
      SQLQueryBuilder(db)
      << "INSERT INTO Metadata(field, value) "
         "VALUES(" << SQLType::kInteger << ", " << SQLType::kText << ")";
  query << static_cast<int64_t>(MetadataField::kDatabaseVersion);
  query << std::string(Database::version);
  query.exec().assertDone();

  // Finish this transaction before setting metadata (which may create its
  // own transactions.
  transaction.commit();

  // Add other metadata now that the database is in a valid state
  // setMetadata();

  MAGEEC_DEBUG("Empty database created");
}

bool Database::appendDatabase(Database &other) {
  assert(this->isCompatible());
  assert(other.isCompatible());

  // TODO: Merge metadata
  // Nothing to merge at the moment
  MAGEEC_DEBUG("Merging metadata");

  // Merge feature type tables
  // TODO: This could be done more efficiently with "ATTACH"
  MAGEEC_DEBUG("Merging feature types and debug");
  SQLQuery select_feature_types(*other.m_db,
      "SELECT feature_id, feature_type FROM FeatureType");
  SQLQuery insert_feature_types =
      SQLQueryBuilder(*m_db)
      << "INSERT OR IGNORE INTO FeatureType(feature_id, feature_type) "
         "VALUES (" << SQLType::kInteger << ", " << SQLType::kInteger << ")";
  for (auto res = select_feature_types.exec(); !res.done(); res = res.next()) {
    assert(res.numColumns() == 2);
    // Ignore duplicate feature_id rows
    insert_feature_types.clearAllBindings();
    insert_feature_types << res.getInteger(0);
    insert_feature_types << res.getInteger(1);
    insert_feature_types.exec().assertDone();
  }
  // Merge feature debug tables
  SQLQuery select_feature_debug(*other.m_db,
      "SELECT feature_id, name FROM FeatureDebug");
  SQLQuery insert_feature_debug =
      SQLQueryBuilder(*m_db)
      << "INSERT OR IGNORE INTO FeatureDebug(feature_id, name) "
         "VALUES (" << SQLType::kInteger << ", " << SQLType::kText << ")";
  for (auto res = select_feature_debug.exec(); !res.done(); res = res.next()) {
    assert(res.numColumns() == 2);
    insert_feature_debug.clearAllBindings();
    insert_feature_debug << res.getInteger(0);
    insert_feature_debug << res.getText(1);
    insert_feature_debug.exec().assertDone();
  }
  // Merge parameter type tables
  MAGEEC_DEBUG("Merging parameter types and debug");
  SQLQuery select_param_types(*other.m_db,
      "SELECT parameter_id, parameter_type FROM ParameterType");
  SQLQuery insert_param_types =
      SQLQueryBuilder(*m_db)
      << "INSERT OR IGNORE INTO ParameterType(parameter_id, parameter_type) "
         "VALUES (" << SQLType::kInteger << ", " << SQLType::kInteger << ")";
  for (auto res = select_param_types.exec(); !res.done(); res = res.next()) {
    assert(res.numColumns() == 2);
    insert_param_types.clearAllBindings();
    insert_param_types << res.getInteger(0);
    insert_param_types << res.getInteger(1);
    insert_param_types.exec().assertDone();
  }
  // Merge parameter debug tables
  SQLQuery select_param_debug(*other.m_db,
      "SELECT parameter_id, name FROM ParameterDebug");
  SQLQuery insert_param_debug =
      SQLQueryBuilder(*m_db)
      << "INSERT OR IGNORE INTO ParameterDebug(parameter_id, name) "
         "VALUES (" << SQLType::kInteger << ", " << SQLType::kText << ")";
  for (auto res = select_param_debug.exec(); !res.done(); res = res.next()) {
    assert(res.numColumns() == 2);
    insert_param_debug.clearAllBindings();
    insert_param_debug << res.getInteger(0);
    insert_param_debug << res.getText(1);
    insert_param_debug.exec().assertDone();
  }

  std::map<FeatureSetID,   FeatureSetID>   feature_set_id_remapping;
  std::map<ParameterSetID, ParameterSetID> parameter_set_id_remapping;
  std::map<CompilationID,  CompilationID>  compilation_id_remapping;

  // For each feature set, get the feature set id and the features from the
  // database to be merged. Add to the new feature set, and store the
  // remapping.
  MAGEEC_DEBUG("Merging features");
  SQLQuery select_feature_set_id(*other.m_db,
      "SELECT feature_set_id FROM FeatureSetFeature");
  std::vector<FeatureSetID> feature_set_ids;
  for (auto res = select_feature_set_id.exec(); !res.done(); res = res.next()) {
    assert(res.numColumns() == 1);
    feature_set_ids.push_back(static_cast<FeatureSetID>(res.getInteger(0)));
  }
  for (auto id : feature_set_ids) {
    FeatureSet features = other.getFeatureSetFeatures(id);
    FeatureSetID new_id = this->newFeatureSet(features);
    feature_set_id_remapping.emplace(id, new_id);
  }

  // For each parameter set, get the parameter set id and the parametes from the
  // database to be merged. Add to the new parameter set, and store the
  // remapping.
  MAGEEC_DEBUG("Merging parameters");
  SQLQuery select_param_set_id(*other.m_db,
      "SELECT parameter_set_id FROM ParameterSetParameter");
  std::vector<ParameterSetID> parameter_set_ids;
  for (auto res = select_param_set_id.exec(); !res.done(); res = res.next()) {
    assert(res.numColumns() == 1);
    parameter_set_ids.push_back(static_cast<ParameterSetID>(res.getInteger(0)));
  }
  for (auto id : parameter_set_ids) {
    auto parameters = other.getParameters(id);
    auto new_id = this->newParameterSet(parameters);
    parameter_set_id_remapping.emplace(id, new_id);
  }

  // For each compilation id, get the compilation id and compilation
  // Update the feature set id and parameter set id insert into the
  // database and store the compilation id remapping
  MAGEEC_DEBUG("Merging compilations");
  SQLQuery select_compilation(*other.m_db,
      "SELECT Compilation.compilation_id, Compilation.feature_set_id, "
             "Compilation.feature_class_id, "
             "Compilation.parameter_set_id, "
             "CompilationDebug.name, CompilationDebug.type, "
             "CompilationDebug.command, CompilationDebug.parent_id "
      "FROM Compilation, CompilationDebug "
      "WHERE Compilation.compilation_id = CompilationDebug.compilation_id");

  SQLQuery insert_compilation =
      SQLQueryBuilder(*m_db)
      << "INSERT INTO Compilation(feature_set_id, feature_class_id, "
                                 "parameter_set_id) "
         "VALUES (" << SQLType::kInteger << ", " << SQLType::kInteger << ", "
                    << SQLType::kInteger << ")";
  SQLQuery insert_compilation_debug =
      SQLQueryBuilder(*m_db)
      << "INSERT INTO CompilationDebug(compilation_id, name, type, command, "
                                      "parent_id) "
         "VALUES (" << SQLType::kInteger << ", "
                    << SQLType::kText << ", "
                    << SQLType::kText << ", "
                    << SQLType::kText << ", "
                    << SQLType::kInteger << ")";
  {
    // Insert all compilations in one big transaction
    SQLTransaction compilation_transaction(m_db);
    for (auto res = select_compilation.exec(); !res.done(); res = res.next()) {
      assert(res.numColumns() == 8);
      auto compilation_id   = static_cast<CompilationID>(res.getInteger(0));
      auto feature_set_id   = static_cast<FeatureSetID>(res.getInteger(1));
      auto feature_class    = static_cast<FeatureClass>(res.getInteger(2));
      auto parameter_set_id = static_cast<ParameterSetID>(res.getInteger(3));
      auto name = res.getText(4);
      auto type = res.getText(5);
      util::Option<std::string> command;
      if (!res.isNull(6))
        command = res.getText(6);
      util::Option<CompilationID> parent;
      if (!res.isNull(7))
        parent = static_cast<CompilationID>(res.getInteger(7));

      // Update to point at the newly inserted features and parameters
      auto new_feature_set_id   = feature_set_id_remapping[feature_set_id];
      auto new_parameter_set_id = parameter_set_id_remapping[parameter_set_id];

      insert_compilation.clearAllBindings();
      insert_compilation << static_cast<int64_t>(new_feature_set_id);
      insert_compilation << static_cast<int64_t>(feature_class);
      insert_compilation << static_cast<int64_t>(new_parameter_set_id);
      insert_compilation.exec().assertDone();

      auto new_compilation_id =
          static_cast<CompilationID>(sqlite3_last_insert_rowid(m_db));

      insert_compilation_debug.clearAllBindings();
      insert_compilation_debug << static_cast<int64_t>(new_compilation_id);
      insert_compilation_debug << name;
      insert_compilation_debug << type;
      if (command)
        insert_compilation_debug << command.get();
      else
        insert_compilation_debug << nullptr;
      if (parent)
        insert_compilation_debug << static_cast<int64_t>(parent.get());
      else
        insert_compilation_debug << nullptr;
      insert_compilation_debug.exec().assertDone();

      // Store the remapping between the compilation ids
      compilation_id_remapping.emplace(compilation_id, new_compilation_id);
    }
    compilation_transaction.commit();
  }



  // Extract, update and reinsert the results data
  MAGEEC_DEBUG("Merging results");
  SQLQuery select_results(*other.m_db,
      "SELECT compilation_id, metric, result FROM Result");
  std::map<std::pair<CompilationID, std::string>, double> new_results;
  for (auto res = select_results.exec(); !res.done(); res = res.next()) {
    assert(res.numColumns() == 3);
    auto compilation_id = static_cast<CompilationID>(res.getInteger(0));
    auto metric = res.getText(1);
    auto result = static_cast<double>(res.getReal(2));

    // Remap the compilation id
    auto new_compilation_id = compilation_id_remapping[compilation_id];
    new_results.emplace(std::make_pair(new_compilation_id, metric), result);
  }
  addResults(new_results);

  // Copy across the machine learner training blob, ignore blobs which
  // already exist
  MAGEEC_DEBUG("Merging machine learners");
  SQLQuery select_ml(*other.m_db,
      "SELECT ml_id, feature_class_id, metric, ml_blob FROM MachineLearner");
  SQLQuery insert_ml =
      SQLQueryBuilder(*m_db)
      << "INSERT OR IGNORE INTO MachineLearner(ml_id, feature_class_id, "
                                              "metric, ml_blob) "
         "VALUES (" << SQLType::kText << ", " << SQLType::kInteger << ", "
                    << SQLType::kText << ", " << SQLType::kBlob << ")";
  for (auto res = select_ml.exec(); !res.done(); res = res.next()) {
    assert(res.numColumns() == 4);
    insert_ml.clearAllBindings();
    insert_ml << res.getText(0);
    insert_ml << res.getInteger(1);
    insert_ml << res.getText(2);
    insert_ml << res.getBlob(3);
    insert_ml.exec().assertDone();
  }
  return true;
}

void Database::validate(void) {
  assert(isCompatible() && "Cannot validate incompatible database!");
}

bool Database::isCompatible(void) {
  util::Version db_version = getVersion();
  return (db_version == Database::version);
}

util::Version Database::getVersion(void) {
  std::string str = getMetadata(MetadataField::kDatabaseVersion);

  unsigned major, minor, patch;
  int res = sscanf(str.c_str(), "%u.%u.%u", &major, &minor, &patch);

  assert(res == 3 && "Database has a malformed version number");
  return util::Version(major, minor, patch);
}

std::vector<TrainedML> Database::getTrainedMachineLearners(void) {
  assert(isCompatible());

  // Store for each of the trained machine learners as they are extracted
  // from the database.
  std::vector<TrainedML> trained_mls;

  SQLQuery query =
      SQLQueryBuilder(*m_db)
      << "SELECT feature_class_id, metric, ml_blob FROM MachineLearner "
         "WHERE ml_id = " << SQLType::kText;

  for (auto I : m_mls) {
    const std::string ml_name = I.first;
    IMachineLearner &ml = *I.second;

    query.clearAllBindings();
    query << ml_name;

    auto res = query.exec();
    while (!res.done()) {
      if (res.numColumns() == 3) {
        FeatureClass feature_class =
            static_cast<FeatureClass>(res.getInteger(0));
        std::string metric = res.getText(1);
        std::vector<uint8_t> ml_blob = res.getBlob(2);

        TrainedML trained_ml(ml, feature_class, metric, ml_blob);
        trained_mls.push_back(trained_ml);
      } else {
        assert(res.numColumns() == 0);
      }
      res = res.next();
    }
  }
  return trained_mls;
}

void Database::garbageCollect() {
  // Delete everything which is not reachable through a result value.
  // If a compilation does not have a result, then all of its features can
  // and parameters can be deleted if they are not reachable through some
  // other means.
  SQLTransaction transaction(m_db);

  MAGEEC_DEBUG("Deleting unused compilations")
  SQLQuery gc_compilations(*m_db,
      "DELETE FROM Compilation WHERE compilation_id NOT IN "
             "(SELECT DISTINCT compilation_id FROM Result)");
  gc_compilations.exec().assertDone();

  MAGEEC_DEBUG("Deleting unused features")
  SQLQuery gc_features(*m_db,
      "DELETE FROM FeatureSetFeature WHERE feature_set_id NOT IN "
             "(SELECT DISTINCT feature_set_id FROM Compilation)");
  gc_features.exec().assertDone();

  MAGEEC_DEBUG("Deleting unused parameters")
  SQLQuery gc_parameters(*m_db,
      "DELETE FROM ParameterSetParameter WHERE parameter_set_id NOT IN "
             "(SELECT DISTINCT parameter_set_id FROM Compilation)");
  gc_parameters.exec().assertDone();

  transaction.commit();
}

std::string Database::getMetadata(MetadataField field) {
  std::string value;

  SQLQuery query =
      SQLQueryBuilder(*m_db)
      << "SELECT value FROM Metadata WHERE field = " << SQLType::kInteger;
  query << static_cast<int64_t>(field);

  SQLQueryIterator res = query.exec();
  if (!res.done()) {
    assert(res.numColumns() == 1);
    value = res.getText(0);

    res.next().assertDone();
  }
  return value;
}

void Database::setMetadata(MetadataField field, std::string value) {
  assert(isCompatible());

  SQLQuery query =
      SQLQueryBuilder(*m_db)
      << "INSERT INTO Metadata(field, value) "
         "VALUES(" << SQLType::kText << ", " << SQLType::kInteger << ")";
  query << static_cast<int64_t>(field) << value;

  query.exec().assertDone();
}

//===------------------- Feature extractor interface-----------------------===//

FeatureSetID Database::newFeatureSet(FeatureSet features) {
  SQLQuery get_feature_set =
      SQLQueryBuilder(*m_db)
      << "SELECT feature_set_id FROM FeatureSetFeature "
         "WHERE feature_set_id = " << SQLType::kInteger;

  // FIXME: This should check that the types are identical if a conflict
  // arises
  SQLQuery insert_feature_type =
      SQLQueryBuilder(*m_db)
      << "INSERT OR IGNORE INTO FeatureType(feature_id, feature_type) "
         "VALUES (" << SQLType::kInteger << ", " << SQLType::kInteger << ")";

  SQLQuery insert_feature =
      SQLQueryBuilder(*m_db)
      << "INSERT INTO FeatureSetFeature(feature_set_id, feature_id, value) "
      << "VALUES (" << SQLType::kInteger << ", " << SQLType::kInteger << ", "
                    << SQLType::kBlob << ")";

  // FIXME: This should check that the keys are identical if a conflict
  // arises.
  SQLQuery insert_feature_debug =
      SQLQueryBuilder(*m_db)
      << "INSERT OR IGNORE INTO FeatureDebug(feature_id, name) "
         "VALUES (" << SQLType::kInteger << ", " << SQLType::kText << ")";

  // The hash of the feature set forms its identifier in the database
  FeatureSetID feature_set_id = static_cast<FeatureSetID>(features.hash());

  // Do a first check to see if the same feature set already exists. If so
  // we don't have to do any expensive probing
  //
  // If it doesn't exist, then we have to gain an exlusive lock to the database,
  // do a linear probe for the next available id, and then insert the new
  // feature set if it doesn't exist. This is really expensive.
  get_feature_set << static_cast<int64_t>(feature_set_id);
  bool feature_set_id_found = false;

  if (!get_feature_set.exec().done()) {
    // There is already a feature set with that id. Do a full check to
    // see if it is equal
    if (features == getFeatureSetFeatures(feature_set_id))
      feature_set_id_found = true;
  }

  if (!feature_set_id_found) {
    // The quick test did not find an equal feature set, so we now need
    // to probe to either find an equal set, or find somewhere to insert
    // the new feature set.
    //
    // The probe + insert must be done atomically without any other process
    // writing to the database in the meantime, so we require an exclusive
    // lock to the database.
    SQLTransaction transaction(m_db, SQLTransaction::kExclusive);

    while (!feature_set_id_found) {
      get_feature_set.clearAllBindings();
      get_feature_set << static_cast<int64_t>(feature_set_id);
      auto feature_iter = get_feature_set.exec();

      if (feature_iter.done()) {
        // The feature set does not already exist, and we have found a free
        // identifier for it. Do the insertion now
        for (auto I : features) {
          // clear feature bindings for all queries
          insert_feature_type.clearAllBindings();
          insert_feature.clearAllBindings();
          insert_feature_debug.clearAllBindings();

          // Add feature type first if not present
          insert_feature_type << static_cast<int64_t>(I->getID())
                              << static_cast<int64_t>(I->getType());
          insert_feature_type.exec().assertDone();

          // feature insertion
          insert_feature << static_cast<int64_t>(feature_set_id) 
                         << static_cast<int64_t>(I->getID())
                         << I->toBlob();
          insert_feature.exec().assertDone();

          // debug table
          insert_feature_debug << static_cast<int64_t>(I->getID())
                               << I->getName();
          insert_feature_debug.exec().assertDone();
        }
        feature_set_id_found = true;
      } else {
        // Check if the feature set with the given id is equal. If it is
        // we're done, otherwise continuing probing
        if (features == getFeatureSetFeatures(feature_set_id)) {
          feature_set_id_found = true;
        } else {
          mageec::ID tmp = static_cast<mageec::ID>(feature_set_id);
          feature_set_id = static_cast<FeatureSetID>(tmp + 1);
        }
      }
    }
    transaction.commit();
  }
  return feature_set_id;
}

FeatureSet Database::getFeatureSetFeatures(FeatureSetID feature_set) {
  // Get all of the features in a feature set
  SQLQuery select_features =
      SQLQueryBuilder(*m_db)
      << "SELECT FeatureSetFeature.feature_id, FeatureType.feature_type, "
                "FeatureSetFeature.value "
         "FROM FeatureType, FeatureSetFeature "
         "WHERE FeatureType.feature_id = FeatureSetFeature.feature_id "
           "AND FeatureSetFeature.feature_set_id = " << SQLType::kInteger;

  // Retrieve the features
  FeatureSet features;
  select_features << static_cast<int64_t>(feature_set);
  for (auto feature_iter = select_features.exec(); !feature_iter.done();
       feature_iter = feature_iter.next()) {
    assert(feature_iter.numColumns() == 3);

    unsigned feature_id =
        static_cast<unsigned>(feature_iter.getInteger(0));
    FeatureType feature_type =
        static_cast<FeatureType>(feature_iter.getInteger(1));
    auto feature_blob = feature_iter.getBlob(2);

    // TODO: Also retrieve feature names
    switch (feature_type) {
    case FeatureType::kBool:
      features.add(BoolFeature::fromBlob(feature_id, feature_blob, {}));
      break;
    case FeatureType::kInt:
      features.add(IntFeature::fromBlob(feature_id, feature_blob, {}));
      break;
    }
  }
  return features;
}

ParameterSet Database::getParameters(ParameterSetID param_set) {
  // Get all of the parameters in a parameter set
  SQLQuery select_parameters =
      SQLQueryBuilder(*m_db)
      << "SELECT ParameterSetParameter.parameter_id, "
                "ParameterType.parameter_type, "
                "ParameterSetParameter.value "
         "FROM ParameterType, ParameterSetParameter "
         "WHERE ParameterType.parameter_id = ParameterSetParameter.parameter_id "
           "AND ParameterSetParameter.parameter_set_id = " << SQLType::kInteger;

  // Retrieve parameters
  ParameterSet parameters;
  select_parameters << static_cast<int64_t>(param_set);
  for (auto param_iter = select_parameters.exec(); !param_iter.done();
       param_iter = param_iter.next()) {
    assert(param_iter.numColumns() == 3);

    unsigned param_type_id = static_cast<unsigned>(param_iter.getInteger(0));
    ParameterType param_type =
        static_cast<ParameterType>(param_iter.getInteger(1));
    auto param_blob = param_iter.getBlob(2);

    // TODO: Also retrieve parameter names
    switch (param_type) {
    case ParameterType::kBool:
      parameters.add(BoolParameter::fromBlob(param_type_id, param_blob, {}));
      break;
    case ParameterType::kRange:
      parameters.add(RangeParameter::fromBlob(param_type_id, param_blob, {}));
      break;
    case ParameterType::kPassSeq:
      parameters.add(PassSeqParameter::fromBlob(param_type_id, param_blob, {}));
      break;
    }
  }
  return parameters;
}

//===----------------------- Compiler interface ---------------------------===//

CompilationID Database::newCompilation(std::string name, std::string type,
                                       FeatureSetID features,
                                       FeatureClass features_class,
                                       ParameterSetID parameters,
                                       util::Option<std::string> command,
                                       util::Option<CompilationID> parent) {
  SQLQuery insert_into_compilation =
      SQLQueryBuilder(*m_db)
      << "INSERT INTO Compilation(feature_set_id, feature_class_id, "
                                 "parameter_set_id) "
         "VALUES (" << SQLType::kInteger << ", " << SQLType::kInteger << ", "
                    << SQLType::kInteger << ")";

  SQLQuery insert_compilation_debug =
      SQLQueryBuilder(*m_db)
      << "INSERT INTO CompilationDebug(compilation_id, name, type, command, "
                                      "parent_id) "
         "VALUES(" << SQLType::kInteger << ", "
                   << SQLType::kText << ", "
                   << SQLType::kText << ", "
                   << SQLType::kText << ", "
                   << SQLType::kInteger << ")";

  // add in a single transaction
  SQLTransaction transaction(m_db);

  // Add the compilation
  insert_into_compilation << static_cast<int64_t>(features)
                          << static_cast<int64_t>(features_class)
                          << static_cast<int64_t>(parameters);
  insert_into_compilation.exec().assertDone();

  // The rowid of the insert is the compilation_id, retrieve it
  int64_t row_id = sqlite3_last_insert_rowid(m_db);
  CompilationID compilation_id = static_cast<CompilationID>(row_id);
  assert(row_id != 0 && "compilation_id overflow");

  // Add debug information
  insert_compilation_debug << static_cast<int64_t>(compilation_id) << name
                           << type;

  if (command) {
    insert_compilation_debug << command.get();
  } else {
    insert_compilation_debug << nullptr;
  }
  if (parent) {
    insert_compilation_debug << static_cast<int64_t>(parent.get());
  } else {
    insert_compilation_debug << nullptr;
  }
  insert_compilation_debug.exec().assertDone();

  transaction.commit();
  return compilation_id;
}

ParameterSetID Database::newParameterSet(ParameterSet parameters) {
  SQLQuery get_parameter_set =
      SQLQueryBuilder(*m_db)
      << "SELECT parameter_set_id FROM ParameterSetParameter "
         "WHERE parameter_set_id = " << SQLType::kInteger;

  // FIXME: This should check that the values are identical if a conflict arises
  SQLQuery insert_parameter_type =
      SQLQueryBuilder(*m_db)
      << "INSERT OR IGNORE INTO ParameterType(parameter_id, parameter_type) "
         "VALUES (" << SQLType::kInteger << ", " << SQLType::kInteger << ")";

  SQLQuery insert_parameter =
      SQLQueryBuilder(*m_db)
      << "INSERT INTO ParameterSetParameter(parameter_set_id, parameter_id, value) "
         "VALUES (" << SQLType::kInteger << ", " << SQLType::kInteger << ", "
                    << SQLType::kBlob << ")";

  // FIXME: This should check that the keys are identical if a conflict
  // arises.
  SQLQuery insert_parameter_debug =
      SQLQueryBuilder(*m_db)
      << "INSERT OR IGNORE INTO ParameterDebug(parameter_id, name) "
         "VALUES (" << SQLType::kInteger << ", " << SQLType::kText << ")";

  // The hash of the parameter set forms its identifier in the database
  ParameterSetID param_set_id = static_cast<ParameterSetID>(parameters.hash());

  // Do a first check to see if the same parameter set already exists. If so
  // we don't have to do any expensive probing
  //
  // If it doesn't exist, then we have to gain an exlusive lock to the database,
  // do a linear probe for the first available id, and then insert the new
  // parameter set. This is really expensive.
  get_parameter_set << static_cast<int64_t>(param_set_id);
  bool param_set_id_found = false;

  if (!get_parameter_set.exec().done()) {
    // There is already a parameter set with that id. Do a full check to
    // see if it is equal
    if (parameters == getParameters(param_set_id))
      param_set_id_found = true;
  }

  if (!param_set_id_found) {
    // The quick test did not find an equal parameter set, so we now need
    // to probe to either find an equal set, or find somewhere to insert
    // the new parameter set.
    //
    // The probe + insert must be done atomically without any other process
    // writing to the database in the meantime, so we require an exclusive
    // lock to the database.
    SQLTransaction transaction(m_db, SQLTransaction::kExclusive);

    while (!param_set_id_found) {
      get_parameter_set.clearAllBindings();
      get_parameter_set << static_cast<int64_t>(param_set_id);
      auto param_iter = get_parameter_set.exec();

      if (param_iter.done()) {
        // The parameter set does not already exist, and we have found a free
        // identifier for it. Do the insertion now
        for (auto I : parameters) {
          // clear parameters bindings for all queries
          insert_parameter_type.clearAllBindings();
          insert_parameter.clearAllBindings();
          insert_parameter_debug.clearAllBindings();

          // add parameter type first if not present
          insert_parameter_type << static_cast<int64_t>(I->getID())
                                << static_cast<int64_t>(I->getType());
          insert_parameter_type.exec().assertDone();

          // parameter insertion
          insert_parameter << static_cast<int64_t>(param_set_id)
                           << static_cast<int64_t>(I->getID())
                           << I->toBlob();
          insert_parameter.exec().assertDone();

          // debug table
          insert_parameter_debug << static_cast<int64_t>(I->getID())
                                 << I->getName();
          insert_parameter_debug.exec().assertDone();
        }
        param_set_id_found = true;
      } else {
        // Check if the parameter set with the given id is equal. If it is
        // we're done, otherwise continuing probing
        if (parameters == getParameters(param_set_id)) {
          param_set_id_found = true;
        } else {
          mageec::ID tmp = static_cast<mageec::ID>(param_set_id);
          param_set_id = static_cast<ParameterSetID>(tmp + 1);
        }
      }
    }
    transaction.commit();
  }
  return param_set_id;
}

//===------------------------ Results interface ---------------------------===//

void Database::
addResults(std::map<std::pair<CompilationID, std::string>, double> results) {
  // It is possible for the user to provide a compilation_id and metric which
  // already has a result in the database. In this case, we replace the
  // original value.
  SQLQuery insert_result =
      SQLQueryBuilder(*m_db)
      << "INSERT OR REPLACE INTO Result(compilation_id, metric, result) "
         "VALUES(" << SQLType::kInteger << ", " << SQLType::kText << ", "
                   << SQLType::kReal << ")";

  SQLQuery get_compilation_ids(
      *m_db, "SELECT compilation_id FROM Compilation");

  // Get all compilation ids first, so that we don't try and insert a
  // result for a compilation which doesn't exist
  std::set<int64_t> compilation_ids;
  for (auto compilation_id_iter = get_compilation_ids.exec();
       !compilation_id_iter.done();
       compilation_id_iter = compilation_id_iter.next()) {
    assert(compilation_id_iter.numColumns() == 1);
    uint64_t id = compilation_id_iter.getInteger(0);
    compilation_ids.insert(id);
  }

  // All results in a single transaction
  SQLTransaction transaction(m_db);

  for (const auto &res : results) {
    auto id = res.first.first;
    auto metric = res.first.second;
    auto value = res.second;

    // Check that this is a result for a valid compilation id, else we will
    // violate a foreign key constraint when adding it to the database
    if (compilation_ids.count(static_cast<int64_t>(id)) != 1) {
      MAGEEC_DEBUG("Result for an invalid compilation id... Ignoring...");
      continue;
    }

    insert_result.clearAllBindings();
    insert_result << static_cast<int64_t>(id) << metric
                  << value;
    insert_result.exec().assertDone();
  }
  transaction.commit();
}

//===----------------------- Training interface ---------------------------===//

void Database::trainMachineLearner(std::string ml, FeatureClass feature_class,
                                   std::string metric) {
  // Get all of the feature types and parameter types, even if some of them
  // don't occur for this metric. These will all be distinct.
  SQLQuery select_feature_types(
      *m_db, "SELECT feature_id, feature_type FROM FeatureType");
  SQLQuery select_parameter_types(
      *m_db, "SELECT parameter_id, parameter_type FROM ParameterType");

  std::string param_type =
      std::to_string(static_cast<unsigned>(ParameterType::kPassSeq));
  SQLQuery select_pass_sequences =
      SQLQueryBuilder(*m_db)
      << "SELECT DISTINCT value FROM ParameterSetParameter, ParameterType "
         "WHERE ParameterSetParameter.parameter_id = ParameterType.parameter_id "
           "AND ParameterType.parameter_type = " << param_type;

  // Insert a blob for the provided machine learner and metric
  // This will fail if there is already training data
  SQLQuery insert_blob =
      SQLQueryBuilder(*m_db)
      << "INSERT OR REPLACE INTO MachineLearner(ml_id, feature_class_id, "
                                               "metric, ml_blob) "
         "VALUES (" << SQLType::kText << ", " << SQLType::kInteger << ", "
                    << SQLType::kText << ", " << SQLType::kBlob << ")";

  // Get the machine learner interface
  auto res = m_mls.find(ml);
  assert(res != m_mls.end() && "Cannot train an unregistered machine learner");
  const IMachineLearner &i_ml = *res->second;

  // Collect all information in a single transaction
  SQLTransaction transaction(m_db);

  std::set<FeatureDesc> feature_descs;
  std::set<ParameterDesc> parameter_descs;
  std::set<std::string> pass_names;

  for (auto feat_iter = select_feature_types.exec(); !feat_iter.done();
       feat_iter = feat_iter.next()) {
    assert(feat_iter.numColumns() == 2);
    FeatureDesc desc = {static_cast<unsigned>(feat_iter.getInteger(0)),
                        static_cast<FeatureType>(feat_iter.getInteger(1))};
    feature_descs.insert(desc);
  }

  for (auto param_iter = select_parameter_types.exec(); !param_iter.done();
       param_iter = param_iter.next()) {
    assert(param_iter.numColumns() == 2);
    ParameterDesc desc = {static_cast<unsigned>(param_iter.getInteger(0)),
                          static_cast<ParameterType>(param_iter.getInteger(1))};
    parameter_descs.insert(desc);
  }

  for (auto pass_seq_iter = select_pass_sequences.exec(); !pass_seq_iter.done();
       pass_seq_iter = pass_seq_iter.next()) {
    assert(pass_seq_iter.numColumns() == 1);

    // Split on commas, add each encountered pass to the set
    std::vector<uint8_t> blob = pass_seq_iter.getBlob(0);
    std::string pass;
    for (auto c : blob) {
      if (c == ',') {
        pass_names.insert(pass);
        pass = std::string();
      } else {
        pass.push_back(static_cast<char>(c));
      }
    }
    pass_names.insert(pass);
  }
  transaction.commit();

  // Iterator to select each set of results in turn
  ResultIterator results(*this, *m_db, feature_class, metric);

  // Retrieve the blob and then insert it into the database
  auto blob = i_ml.train(feature_descs, parameter_descs, pass_names,
                         std::move(results));

  // FIXME: Handle case where the blob is empty. (causes a failure when
  // running the database query).

  insert_blob << ml << static_cast<int64_t>(feature_class) << metric << blob;
  insert_blob.exec().assertDone();
}

//===------------------------ Result Iterator -----------------------------===//

ResultIterator::ResultIterator(Database &db, sqlite3 &raw_db,
                               FeatureClass feature_class,
                               std::string metric)
    : m_db(&db) {
  // Get each compilation and its accompanying results
  SQLQueryBuilder select_compilation_result =
      SQLQueryBuilder(raw_db)
      << "SELECT Compilation.feature_set_id, Compilation.parameter_set_id, "
                "Result.result "
         "FROM Compilation, Result "
         "WHERE Compilation.compilation_id = Result.compilation_id "
           "AND Compilation.feature_class_id = " << SQLType::kInteger << " "
           "AND Result.metric = " << SQLType::kText << " "
         "ORDER BY Compilation.compilation_id";
  m_query.reset(new SQLQuery(select_compilation_result));
  *m_query << static_cast<int64_t>(feature_class);
  *m_query << metric;;

  m_result_iter.reset(new SQLQueryIterator(m_query->exec()));
}

ResultIterator::ResultIterator(ResultIterator &&other)
    : m_db(other.m_db),
      m_query(std::move(other.m_query)),
      m_result_iter(std::move(other.m_result_iter)) {
  other.m_db = nullptr;
}

ResultIterator &ResultIterator::operator=(ResultIterator &&other) {
  m_db = other.m_db;
  m_query = std::move(other.m_query);
  m_result_iter = std::move(other.m_result_iter);

  other.m_db = nullptr;
  return *this;
}

util::Option<Result> ResultIterator::operator*() {
  if (m_result_iter->done())
    return util::Option<Result>();

  assert(m_result_iter->numColumns() == 3);

  FeatureSet features;
  ParameterSet parameters;

  auto feature_set = static_cast<FeatureSetID>(m_result_iter->getInteger(0));
  features = m_db->getFeatureSetFeatures(feature_set);
  assert(features.size() != 0);

  if (!m_result_iter->isNull(1)) {
    auto param_set = static_cast<ParameterSetID>(m_result_iter->getInteger(1));
    parameters = m_db->getParameters(param_set);
    assert(parameters.size() != 0);
  }

  auto res = m_result_iter->getReal(2);
  return Result(features, parameters, res);
}

ResultIterator ResultIterator::next() {
  if (m_result_iter->done()) {
    return std::move(*this);
  } else {
    *m_result_iter = m_result_iter->next();
    return std::move(*this);
  }
}

SQLTransaction::SQLTransaction(sqlite3 *db, TransactionType type)
    : m_is_committed(false), m_db(db) {
  const char *query_str;
  if (type == kImmediate) {
    query_str = "BEGIN IMMEDIATE TRANSACTION";
  } else if (type == kExclusive) {
    query_str = "BEGIN EXCLUSIVE TRANSACTION";
  } else {
    assert (type == kDeferred);
    query_str = "BEGIN TRANSACTION";
  }
  SQLQuery start_transaction(*m_db, query_str);
  start_transaction.exec().assertDone();
  m_is_init = true;
}

SQLTransaction::~SQLTransaction() {
  if (m_is_init && !m_is_committed) {
    SQLQuery rollback_transaction(*m_db, "ROLLBACK");
    rollback_transaction.exec().assertDone();
  }
}

SQLTransaction::SQLTransaction(SQLTransaction &&other)
    : m_is_committed(std::move(other.m_is_committed)),
      m_db(std::move(other.m_db)) {
  assert(other.m_is_init && "Cannot move from a transaction which has already "
                            "been moved");
  other.m_is_init = false;
  m_is_init = true;
}

SQLTransaction &SQLTransaction::
operator=(SQLTransaction &&other) {
  assert(other.m_is_init && "Cannot move from a transaction which has already "
                            "been moved");

  m_is_committed = std::move(other.m_is_committed);
  m_db = std::move(other.m_db);

  other.m_is_init = false;
  m_is_init = true;
  return *this;
}

void SQLTransaction::commit() {
  assert(m_is_init && "Transaction has been moved");
  assert(!m_is_committed && "Transaction has already been committed");

  SQLQuery commit_transaction(*m_db, "COMMIT");
  commit_transaction.exec().assertDone();
  m_is_committed = true;
}

} // end of namespace mageec
