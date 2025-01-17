// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/interest_group/interest_group_storage.h"

#include <memory>
#include <vector>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/json/json_string_value_serializer.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/strings/string_piece.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "base/util/values/values_util.h"
#include "content/services/auction_worklet/public/mojom/auction_worklet_service.mojom-forward.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "sql/database.h"
#include "sql/error_delegate_util.h"
#include "sql/meta_table.h"
#include "sql/recovery.h"
#include "sql/statement.h"
#include "sql/transaction.h"
#include "url/origin.h"

namespace content {

namespace {

using auction_worklet::mojom::BiddingBrowserSignalsPtr;
using auction_worklet::mojom::BiddingInterestGroupPtr;
using auction_worklet::mojom::PreviousWinPtr;
using blink::mojom::InterestGroupAdPtr;
using blink::mojom::InterestGroupPtr;

const base::FilePath::CharType kDatabasePath[] =
    FILE_PATH_LITERAL("InterestGroups");

constexpr base::TimeDelta kIdlePeriod = base::TimeDelta::FromSeconds(30);

// Version number of the database.
//
// Version 1 - 2021/03 - crrev.com/c/2757425
//
// Version 1 adds a table for interest groups.
const int kCurrentVersionNumber = 1;

// Earliest version which can use a |kCurrentVersionNumber| database
// without failing.
const int kCompatibleVersionNumber = 1;

// Latest version of the database that cannot be upgraded to
// |kCurrentVersionNumber| without razing the database. No versions are
// currently deprecated.
const int kDeprecatedVersionNumber = 0;

// TODO(crbug.com/1195852): Add UMA to count errors.
}  // namespace

namespace {

std::string Serialize(const base::Value& value) {
  std::string json_output;
  JSONStringValueSerializer serializer(&json_output);
  if (!serializer.Serialize(value))
    LOG(ERROR) << "Could not serialize value:   " << value.DebugString();

  return json_output;
}
std::unique_ptr<base::Value> DeserializeValue(std::string serialized_value) {
  if (serialized_value.empty())
    return {};
  JSONStringValueDeserializer deserializer{base::StringPiece(serialized_value)};
  std::string error_message;
  std::unique_ptr<base::Value> result =
      deserializer.Deserialize(nullptr, &error_message);
  if (!result) {
    LOG(ERROR) << "Could not deserialize value `" << serialized_value
               << "`:   " << error_message;
  }
  return result;
}

std::string Serialize(const url::Origin& origin) {
  return origin.Serialize();
}
url::Origin DeserializeOrigin(const std::string& serialized_origin) {
  return url::Origin::Create(GURL(serialized_origin));
}

std::string Serialize(const base::Optional<GURL>& url) {
  if (!url)
    return "";
  return url->spec();
}
base::Optional<GURL> DeserializeURL(const std::string& serialized_url) {
  GURL result(serialized_url);
  if (result.is_empty())
    return base::nullopt;
  return result;
}

base::Value ToValue(const ::blink::mojom::InterestGroupAd& ad) {
  base::Value dict(base::Value::Type::DICTIONARY);
  dict.SetStringKey("url", ad.render_url.spec());
  if (ad.metadata)
    dict.SetStringKey("metadata", ad.metadata.value());
  return dict;
}
InterestGroupAdPtr FromInterestGroupAdPtrValue(const base::Value* value) {
  InterestGroupAdPtr result = blink::mojom::InterestGroupAd::New();
  if (!value) {
    LOG(ERROR) << "converting InterestGroupAd from value";
    return result;
  }
  const std::string* maybe_url = value->FindStringKey("url");
  if (!maybe_url)
    LOG(ERROR) << "url field not found in serialized InterestGroupAdPtrValue";
  result->render_url = GURL(*maybe_url);
  const std::string* maybe_metadata = value->FindStringKey("metadata");
  if (maybe_metadata)
    result->metadata = *maybe_metadata;
  return result;
}

std::string Serialize(
    const base::Optional<std::vector<InterestGroupAdPtr>>& ads) {
  if (!ads)
    return "";
  base::Value list(base::Value::Type::LIST);
  for (const auto& ad : ads.value()) {
    list.Append(ToValue(*ad));
  }
  return Serialize(list);
}
base::Optional<std::vector<InterestGroupAdPtr>>
DeserializeInterestGroupAdPtrVector(std::string serialized_ads) {
  std::vector<InterestGroupAdPtr> result;
  std::unique_ptr<base::Value> ads_value = DeserializeValue(serialized_ads);
  if (!ads_value) {
    return base::nullopt;
  }
  for (const auto& ad_value : ads_value->GetList())
    result.push_back(FromInterestGroupAdPtrValue(&ad_value));
  return result;
}

std::string Serialize(const base::Optional<std::vector<std::string>> strings) {
  if (!strings)
    return "";
  base::Value list(base::Value::Type::LIST);
  for (const auto& s : strings.value())
    list.Append(s);
  return Serialize(list);
}
base::Optional<std::vector<std::string>> DeserializeStringVector(
    std::string serialized_vector) {
  std::unique_ptr<base::Value> list = DeserializeValue(serialized_vector);
  if (!list)
    return base::nullopt;
  std::vector<std::string> result;
  for (const auto& value : list->GetList())
    result.push_back(value.GetString());
  return result;
}

// Initializes the tables, returning true on success.
// The tables cannot exist when calling this function.
bool CreateV1Schema(sql::Database& db) {
  DCHECK(!db.DoesTableExist("interest_groups"));
  static const char kInterestGroupTableSql[] =
      // clang-format off
      "CREATE TABLE interest_groups("
        "expiration INTEGER NOT NULL,"
        "last_updated INTEGER NOT NULL,"
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "bidding_url TEXT NOT NULL,"
        "update_url TEXT NOT NULL,"
        "trusted_bidding_signals_url TEXT NOT NULL,"
        "trusted_bidding_signals_keys TEXT NOT NULL,"
        "user_bidding_signals TEXT,"
        "ads TEXT NOT NULL,"
      "PRIMARY KEY(owner,name))";
  // clang-format on
  if (!db.Execute(kInterestGroupTableSql))
    return false;

  DCHECK(!db.DoesIndexExist("interest_group_expiration"));
  static const char kInterestGroupExpirationIndexSql[] =
      // clang-format off
      "CREATE INDEX interest_group_expiration"
      " ON interest_groups(expiration DESC)";
  // clang-format on
  if (!db.Execute(kInterestGroupExpirationIndexSql))
    return false;

  // We can't use the interest group and join time as primary keys since
  // different pages may try to join the same interest group at the same time.
  DCHECK(!db.DoesTableExist("join_history"));
  static const char kJoinHistoryTableSql[] =
      // clang-format off
      "CREATE TABLE join_history("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "join_time INTEGER NOT NULL,"
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kJoinHistoryTableSql))
    return false;

  DCHECK(!db.DoesIndexExist("join_history_index"));
  static const char kJoinHistoryIndexSql[] =
      // clang-format off
      "CREATE INDEX join_history_index "
      "ON join_history(owner,name,join_time)";
  // clang-format on
  if (!db.Execute(kJoinHistoryIndexSql))
    return false;

  // We can't use the interest group and bid time as primary keys since
  // auctions on separate pages may occur at the same time.
  DCHECK(!db.DoesTableExist("bid_history"));
  static const char kBidHistoryTableSql[] =
      // clang-format off
      "CREATE TABLE bid_history("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "bid_time INTEGER NOT NULL,"
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kBidHistoryTableSql))
    return false;

  DCHECK(!db.DoesIndexExist("bid_history_index"));
  static const char kBidHistoryIndexSql[] =
      // clang-format off
      "CREATE INDEX bid_history_index "
      "ON bid_history(owner,name,bid_time)";
  // clang-format on
  if (!db.Execute(kBidHistoryIndexSql))
    return false;

  // We can't use the interest group and win time as primary keys since
  // auctions on separate pages may occur at the same time.
  DCHECK(!db.DoesTableExist("win_history"));
  static const char kWinHistoryTableSQL[] =
      // clang-format off
      "CREATE TABLE win_history("
        "owner TEXT NOT NULL,"
        "name TEXT NOT NULL,"
        "win_time INTEGER NOT NULL,"
        "ad TEXT NOT NULL,"
      "FOREIGN KEY(owner,name) REFERENCES interest_groups)";
  // clang-format on
  if (!db.Execute(kWinHistoryTableSQL))
    return false;

  DCHECK(!db.DoesIndexExist("win_history_index"));
  static const char kWinHistoryIndexSQL[] =
      // clang-format off
      "CREATE INDEX win_history_index "
      "ON win_history(owner,name,win_time DESC)";
  // clang-format on
  if (!db.Execute(kWinHistoryIndexSQL))
    return false;

  return true;
}

bool DoJoinInterestGroup(sql::Database& db,
                         const InterestGroupPtr& data,
                         base::Time last_updated) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;

  // clang-format off
  sql::Statement join_group(
      db.GetCachedStatement(SQL_FROM_HERE,
          "INSERT OR REPLACE INTO interest_groups("
            "expiration,"
            "last_updated,"
            "owner,"
            "name,"
            "bidding_url,"
            "update_url,"
            "trusted_bidding_signals_url,"
            "trusted_bidding_signals_keys,"
            "user_bidding_signals,"  // opaque data
            "ads) "
          "VALUES(?,?,?,?,?,?,?,?,?,?)"));
  // clang-format on
  if (!join_group.is_valid())
    return false;

  join_group.Reset(true);
  join_group.BindTime(0, data->expiry);
  join_group.BindTime(1, last_updated);
  join_group.BindString(2, Serialize(data->owner));
  join_group.BindString(3, data->name);
  join_group.BindString(4, Serialize(data->bidding_url));
  join_group.BindString(5, Serialize(data->update_url));
  join_group.BindString(6, Serialize(data->trusted_bidding_signals_url));
  join_group.BindString(7, Serialize(data->trusted_bidding_signals_keys));
  if (data->user_bidding_signals) {
    join_group.BindString(8, data->user_bidding_signals.value());
  } else {
    join_group.BindNull(8);
  }
  join_group.BindString(9, Serialize(data->ads));

  if (!join_group.Run())
    return false;

  // Record the join. It should be unique since a site should only join once
  // per a page load. If it is not unique we should collapse the entries to
  // minimize the damage done by a misbehaving site.
  sql::Statement join_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "INSERT INTO join_history(owner,name,join_time) "
                            "VALUES(?,?,?)"));
  if (!join_hist.is_valid())
    return false;

  join_hist.Reset(true);
  join_hist.BindString(0, Serialize(data->owner));
  join_hist.BindString(1, data->name);
  join_hist.BindTime(2, last_updated);

  if (!join_hist.Run())
    return false;

  return transaction.Commit();
}

bool DoUpdateInterestGroup(sql::Database& db,
                           const InterestGroupPtr& data,
                           base::Time now) {
  // clang-format off
  sql::Statement update_group(
      db.GetCachedStatement(SQL_FROM_HERE,
          "UPDATE interest_groups SET "
            "last_updated=?,"
            "bidding_url=?,"
            "update_url=?,"
            "trusted_bidding_signals_url=?,"
            "trusted_bidding_signals_keys=?,"
            "ads=? "
          "WHERE owner=? AND name=?"));
  // clang-format on
  if (!update_group.is_valid())
    return false;

  update_group.Reset(true);
  update_group.BindTime(0, now);
  update_group.BindString(1, Serialize(data->bidding_url));
  update_group.BindString(2, Serialize(data->update_url));
  update_group.BindString(3, Serialize(data->trusted_bidding_signals_url));
  update_group.BindString(4, Serialize(data->trusted_bidding_signals_keys));
  update_group.BindString(5, Serialize(data->ads));
  update_group.BindString(6, Serialize(data->owner));
  update_group.BindString(7, data->name);

  return update_group.Run();
}

bool RemoveJoinHistory(sql::Database& db,
                       const url::Origin& owner,
                       const std::string& name) {
  sql::Statement remove_join_history(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM join_history "
                            "WHERE owner=? AND name=?"));
  if (!remove_join_history.is_valid())
    return false;

  remove_join_history.Reset(true);
  remove_join_history.BindString(0, Serialize(owner));
  remove_join_history.BindString(1, name);
  return remove_join_history.Run();
}

bool RemoveBidHistory(sql::Database& db,
                      const url::Origin& owner,
                      const std::string& name) {
  sql::Statement remove_bid_history(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM bid_history "
                            "WHERE owner=? AND name=?"));
  if (!remove_bid_history.is_valid())
    return false;

  remove_bid_history.Reset(true);
  remove_bid_history.BindString(0, Serialize(owner));
  remove_bid_history.BindString(1, name);
  return remove_bid_history.Run();
}

bool RemoveWinHistory(sql::Database& db,
                      const url::Origin& owner,
                      const std::string& name) {
  sql::Statement remove_win_history(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM win_history "
                            "WHERE owner=? AND name=?"));
  if (!remove_win_history.is_valid())
    return false;

  remove_win_history.Reset(true);
  remove_win_history.BindString(0, Serialize(owner));
  remove_win_history.BindString(1, name);
  return remove_win_history.Run();
}

bool DoLeaveInterestGroup(sql::Database& db,
                          const url::Origin& owner,
                          const std::string& name) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;

  // These tables have foreign keys that reference the interest group table.
  if (!RemoveJoinHistory(db, owner, name))
    return false;
  if (!RemoveBidHistory(db, owner, name))
    return false;
  if (!RemoveWinHistory(db, owner, name))
    return false;

  sql::Statement leave_group(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "DELETE FROM interest_groups "
                            "WHERE owner=? AND name=?"));
  if (!leave_group.is_valid())
    return false;

  leave_group.Reset(true);
  leave_group.BindString(0, Serialize(owner));
  leave_group.BindString(1, name);
  return leave_group.Run() && transaction.Commit();
}

bool DoRecordInterestGroupBid(sql::Database& db,
                              const url::Origin& owner,
                              const std::string& name,
                              base::Time bid_time) {
  // Record the bid. It should be unique since auctions should be serialized.
  // If it is not unique we should just keep the first one.
  // clang-format off
  sql::Statement bid_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO bid_history(owner,name,bid_time) "
      "VALUES(?,?,?)"));
  // clang-format on
  if (!bid_hist.is_valid())
    return false;

  bid_hist.Reset(true);
  bid_hist.BindString(0, Serialize(owner));
  bid_hist.BindString(1, name);
  bid_hist.BindTime(2, bid_time);
  return bid_hist.Run();
}

bool DoRecordInterestGroupWin(sql::Database& db,
                              const url::Origin& owner,
                              const std::string& name,
                              const std::string& ad_json,
                              base::Time win_time) {
  // Record the win. It should be unique since auctions should be serialized.
  // If it is not unique we should just keep the first one.
  // clang-format off
  sql::Statement win_hist(
      db.GetCachedStatement(SQL_FROM_HERE,
      "INSERT INTO win_history(owner,name,win_time,ad) "
      "VALUES(?,?,?,?)"));
  // clang-format on
  if (!win_hist.is_valid())
    return false;

  win_hist.Reset(true);
  win_hist.BindString(0, Serialize(owner));
  win_hist.BindString(1, name);
  win_hist.BindTime(2, win_time);
  win_hist.BindString(3, ad_json);
  return win_hist.Run();
}

base::Optional<std::vector<url::Origin>> DoGetAllInterestGroupOwners(
    sql::Database& db,
    base::Time expiring_after) {
  std::vector<url::Origin> result;
  // TODO(crbug.com/1197209): Adjust the limits on this query in response to
  // usage.
  sql::Statement load(db.GetCachedStatement(SQL_FROM_HERE,
                                            "SELECT DISTINCT owner "
                                            "FROM interest_groups "
                                            "WHERE expiration >= ?"
                                            "LIMIT 1000"));
  if (!load.is_valid()) {
    DLOG(ERROR) << "LoadAllInterestGroups SQL statement did not compile: "
                << db.GetErrorMessage();
    return base::nullopt;
  }
  load.Reset(true);
  load.BindTime(0, expiring_after);
  while (load.Step()) {
    result.push_back(DeserializeOrigin(load.ColumnString(0)));
  }
  if (!load.Succeeded())
    return base::nullopt;
  return result;
}

bool GetPreviousWins(sql::Database& db,
                     const url::Origin& owner,
                     const std::string& name,
                     base::Time win_time_after,
                     BiddingInterestGroupPtr& output) {
  // clang-format off
  sql::Statement prev_wins(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT win_time, ad "
                            "FROM win_history "
                            "WHERE owner = ? AND name = ? AND win_time >= ? "
                            "ORDER BY win_time DESC"));
  // clang-format on
  if (!prev_wins.is_valid()) {
    DLOG(ERROR) << "GetInterestGroupsForOwner win_history SQL statement did "
                   "not compile: "
                << db.GetErrorMessage();
    return false;
  }
  prev_wins.Reset(true);
  prev_wins.BindString(0, Serialize(owner));
  prev_wins.BindString(1, name);
  prev_wins.BindTime(2, win_time_after);
  while (prev_wins.Step()) {
    PreviousWinPtr prev_win = auction_worklet::mojom::PreviousWin::New();
    prev_win->time = prev_wins.ColumnTime(0);
    prev_win->ad_json = prev_wins.ColumnString(1);
    output->signals->prev_wins.push_back(std::move(prev_win));
  }
  return prev_wins.Succeeded();
}

bool GetJoinCount(sql::Database& db,
                  const url::Origin& owner,
                  const std::string& name,
                  base::Time joined_after,
                  BiddingInterestGroupPtr& output) {
  // clang-format off
  sql::Statement join_count(
      db.GetCachedStatement(SQL_FROM_HERE,
    "SELECT COUNT(1) "
    "FROM join_history "
    "WHERE owner = ? AND name = ? AND join_time >=?"));
  // clang-format on
  if (!join_count.is_valid()) {
    DLOG(ERROR) << "GetJoinCount SQL statement did not compile: "
                << db.GetErrorMessage();
    return false;
  }
  join_count.Reset(true);
  join_count.BindString(0, Serialize(owner));
  join_count.BindString(1, name);
  join_count.BindTime(2, joined_after);
  while (join_count.Step()) {
    output->signals->join_count = join_count.ColumnInt64(0);
  }
  return join_count.Succeeded();
}

bool GetBidCount(sql::Database& db,
                 const url::Origin& owner,
                 const std::string& name,
                 base::Time now,
                 BiddingInterestGroupPtr& output) {
  // clang-format off
  sql::Statement bid_count(
      db.GetCachedStatement(SQL_FROM_HERE,
    "SELECT COUNT(1) "
    "FROM bid_history "
    "WHERE owner = ? AND name = ? AND bid_time >= ?"));
  // clang-format on
  if (!bid_count.is_valid()) {
    DLOG(ERROR) << "GetBidCount SQL statement did not compile: "
                << db.GetErrorMessage();
    return false;
  }
  bid_count.Reset(true);
  bid_count.BindString(0, Serialize(owner));
  bid_count.BindString(1, name);
  bid_count.BindTime(2, now - InterestGroupStorage::kHistoryLength);
  while (bid_count.Step()) {
    output->signals->bid_count = bid_count.ColumnInt64(0);
  }
  return bid_count.Succeeded();
}

base::Optional<std::vector<BiddingInterestGroupPtr>>
DoGetInterestGroupsForOwner(sql::Database& db,
                            const url::Origin& owner,
                            base::Time now) {
  std::vector<BiddingInterestGroupPtr> result;
  // TODO(crbug.com/1197209): Adjust the limits on this query in response to
  // usage.
  // clang-format off
  sql::Statement load(
      db.GetCachedStatement(SQL_FROM_HERE,
        "SELECT expiration,"
          "last_updated,"
          "owner,"
          "name,"
          "bidding_url,"
          "update_url,"
          "trusted_bidding_signals_url,"
          "trusted_bidding_signals_keys,"
          "user_bidding_signals,"  // opaque data
          "ads "
        "FROM interest_groups "
        "WHERE owner = ? AND expiration >=? "
        "ORDER BY expiration ASC "
        "LIMIT 1000"));
  // clang-format on

  if (!load.is_valid()) {
    DLOG(ERROR) << "GetInterestGroupsForOwner SQL statement did not compile: "
                << db.GetErrorMessage();
    return base::nullopt;
  }

  load.Reset(true);
  load.BindString(0, Serialize(owner));
  load.BindTime(1, now);
  sql::Transaction transaction(&db);

  if (!transaction.Begin())
    return base::nullopt;
  while (load.Step()) {
    BiddingInterestGroupPtr bidding_interest_group =
        auction_worklet::mojom::BiddingInterestGroup::New();
    InterestGroupPtr interest_group = blink::mojom::InterestGroup::New();

    interest_group->expiry = load.ColumnTime(0);
    interest_group->owner = DeserializeOrigin(load.ColumnString(2));
    interest_group->name = load.ColumnString(3);
    interest_group->bidding_url = DeserializeURL(load.ColumnString(4));
    interest_group->update_url = DeserializeURL(load.ColumnString(5));
    interest_group->trusted_bidding_signals_url =
        DeserializeURL(load.ColumnString(6));
    interest_group->trusted_bidding_signals_keys =
        DeserializeStringVector(load.ColumnString(7));
    if (load.GetColumnType(8) != sql::ColumnType::kNull)
      interest_group->user_bidding_signals = load.ColumnString(8);
    interest_group->ads =
        DeserializeInterestGroupAdPtrVector(load.ColumnString(9));
    bidding_interest_group->group = std::move(interest_group);

    bidding_interest_group->signals =
        auction_worklet::mojom::BiddingBrowserSignals::New();
    result.push_back(std::move(bidding_interest_group));
  }
  if (!load.Succeeded())
    return base::nullopt;

  // These queries are in separate loops to improve locality of the database
  // access.
  for (auto& bidding_interest_group : result) {
    if (!GetJoinCount(db, owner, bidding_interest_group->group->name,
                      now - InterestGroupStorage::kHistoryLength,
                      bidding_interest_group)) {
      return base::nullopt;
    }
  }
  for (auto& bidding_interest_group : result) {
    if (!GetBidCount(db, owner, bidding_interest_group->group->name,
                     now - InterestGroupStorage::kHistoryLength,
                     bidding_interest_group)) {
      return base::nullopt;
    }
  }
  for (auto& bidding_interest_group : result) {
    if (!GetPreviousWins(db, owner, bidding_interest_group->group->name,
                         now - InterestGroupStorage::kHistoryLength,
                         bidding_interest_group)) {
      return base::nullopt;
    }
  }
  if (!transaction.Commit())
    return base::nullopt;
  return result;
}

bool DoDeleteInterestGroupData(
    sql::Database& db,
    const base::RepeatingCallback<bool(const url::Origin&)>& origin_matcher) {
  base::Time infinite_past = base::Time::Min();
  sql::Transaction transaction(&db);

  if (!transaction.Begin())
    return false;

  std::vector<url::Origin> affected_origins;
  base::Optional<std::vector<url::Origin>> maybe_all_origins =
      DoGetAllInterestGroupOwners(db, infinite_past);
  if (!maybe_all_origins)
    return false;
  for (const url::Origin& origin : maybe_all_origins.value()) {
    if (origin_matcher.is_null() || origin_matcher.Run(origin)) {
      affected_origins.push_back(origin);
    }
  }

  for (const auto& affected_origin : affected_origins) {
    base::Optional<std::vector<BiddingInterestGroupPtr>> maybe_interest_groups =
        DoGetInterestGroupsForOwner(db, affected_origin, infinite_past);
    if (!maybe_interest_groups)
      return false;
    for (const auto& bidding_interest_group : maybe_interest_groups.value()) {
      if (!DoLeaveInterestGroup(db, affected_origin,
                                bidding_interest_group->group->name))
        return false;
    }
  }
  return transaction.Commit();
}

bool DeleteOldJoins(sql::Database& db, base::Time cutoff) {
  sql::Statement del_join_history(db.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM join_history WHERE join_time <= ?"));
  if (!del_join_history.is_valid()) {
    DLOG(ERROR) << "DeleteOldJoins SQL statement did not compile.";
    return false;
  }
  del_join_history.Reset(true);
  del_join_history.BindTime(0, cutoff);
  if (!del_join_history.Run()) {
    DLOG(ERROR) << "Could not delete old join_history.";
    return false;
  }
  return true;
}

bool DeleteOldBids(sql::Database& db, base::Time cutoff) {
  sql::Statement del_bid_history(db.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM bid_history WHERE bid_time <= ?"));
  if (!del_bid_history.is_valid()) {
    DLOG(ERROR) << "DeleteOldBids SQL statement did not compile.";
    return false;
  }
  del_bid_history.Reset(true);
  del_bid_history.BindTime(0, cutoff);
  if (!del_bid_history.Run()) {
    DLOG(ERROR) << "Could not delete old bid_history.";
    return false;
  }
  return true;
}

bool DeleteOldWins(sql::Database& db, base::Time cutoff) {
  sql::Statement del_win_history(db.GetCachedStatement(
      SQL_FROM_HERE, "DELETE FROM win_history WHERE win_time <= ?"));
  if (!del_win_history.is_valid()) {
    DLOG(ERROR) << "DeleteOldWins SQL statement did not compile.";
    return false;
  }
  del_win_history.Reset(true);
  del_win_history.BindTime(0, cutoff);
  if (!del_win_history.Run()) {
    DLOG(ERROR) << "Could not delete old win_history.";
    return false;
  }
  return true;
}

bool ClearExpiredInterestGroups(sql::Database& db,
                                base::Time expiration_before) {
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;

  sql::Statement expired_interest_group(
      db.GetCachedStatement(SQL_FROM_HERE,
                            "SELECT owner, name "
                            "FROM interest_groups "
                            "WHERE expiration <= ?"));
  if (!expired_interest_group.is_valid()) {
    DLOG(ERROR) << "ClearExpiredInterestGroups SQL statement did not compile.";
    return false;
  }

  expired_interest_group.Reset(true);
  expired_interest_group.BindTime(0, expiration_before);
  std::vector<std::pair<url::Origin, std::string>> expired_groups;
  while (expired_interest_group.Step()) {
    expired_groups.emplace_back(
        DeserializeOrigin(expired_interest_group.ColumnString(0)),
        expired_interest_group.ColumnString(1));
  }
  if (!expired_interest_group.Succeeded()) {
    DLOG(ERROR) << "ClearExpiredInterestGroups could not get expired groups.";
    // Keep going so we can clear any groups that we did get.
  }
  for (const auto& interest_group : expired_groups) {
    if (!DoLeaveInterestGroup(db, interest_group.first, interest_group.second))
      return false;
  }
  return transaction.Commit();
}

bool DoPerformDatabaseMaintenance(sql::Database& db, base::Time now) {
  SCOPED_UMA_HISTOGRAM_SHORT_TIMER("Storage.InterestGroup.DBMaintenanceTime");
  sql::Transaction transaction(&db);
  if (!transaction.Begin())
    return false;
  if (!ClearExpiredInterestGroups(db, now))
    return false;
  if (!DeleteOldJoins(db, now - InterestGroupStorage::kHistoryLength))
    return false;
  if (!DeleteOldBids(db, now - InterestGroupStorage::kHistoryLength))
    return false;
  if (!DeleteOldWins(db, now - InterestGroupStorage::kHistoryLength))
    return false;
  return transaction.Commit();
}

base::FilePath DBPath(const base::FilePath& base) {
  if (base.empty())
    return base;
  return base.Append(kDatabasePath);
}

}  // namespace

constexpr base::TimeDelta InterestGroupStorage::kHistoryLength;
constexpr base::TimeDelta InterestGroupStorage::kMaintenanceInterval;

InterestGroupStorage::InterestGroupStorage(const base::FilePath& path)
    : db_(std::make_unique<sql::Database>(sql::DatabaseOptions{})),
      path_to_database_(DBPath(path)) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

InterestGroupStorage::~InterestGroupStorage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

bool InterestGroupStorage::EnsureDBInitialized() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  last_access_time_ = base::Time::Now();
  if (db_ && db_->is_open())
    return true;
  return InitializeDB();
}

bool InterestGroupStorage::InitializeDB() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  db_ = std::make_unique<sql::Database>(sql::DatabaseOptions{});
  db_->set_error_callback(base::BindRepeating(
      &InterestGroupStorage::DatabaseErrorCallback, base::Unretained(this)));

  if (path_to_database_.empty()) {
    if (!db_->OpenInMemory()) {
      DLOG(ERROR) << "Failed to create in-memory interest group database: "
                  << db_->GetErrorMessage();
      return false;
    }
  } else {
    const base::FilePath dir = path_to_database_.DirName();

    if (!base::DirectoryExists(dir) && !base::CreateDirectory(dir)) {
      DLOG(ERROR) << "Failed to create directory for interest group database";
      return false;
    }
    if (db_->Open(path_to_database_) == false) {
      DLOG(ERROR) << "Failed to open interest group database: "
                  << db_->GetErrorMessage();
      return false;
    }
  }

  if (!InitializeSchema()) {
    db_->Close();
    return false;
  }

  PerformDBMaintenance();
  return true;
}

bool InterestGroupStorage::InitializeSchema() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!db_)
    return false;

  if (!db_->DoesTableExist("interest_groups")) {
    return CreateV1Schema(*db_);
  }

  sql::MetaTable meta_table;

  if (!meta_table.Init(db_.get(), kCurrentVersionNumber,
                       kCompatibleVersionNumber))
    return false;
  int current_version = meta_table.GetVersionNumber();

  if (current_version == kCurrentVersionNumber)
    return true;

  if (current_version <= kDeprecatedVersionNumber) {
    // Note that this also razes the meta table, so it will need to be
    // initialized again.
    meta_table.Reset();
    db_->Raze();
    return meta_table.Init(db_.get(), kCurrentVersionNumber,
                           kCompatibleVersionNumber) &&
           CreateV1Schema(*db_);
  }

  if (meta_table.GetCompatibleVersionNumber() > kCurrentVersionNumber) {
    // In this case the database version is too new to be used. The DB will
    // never work until Chrome is re-upgraded. Assume the user will continue
    // using this Chrome version and raze the DB to get conversion measurement
    // working.
    meta_table.Reset();
    db_->Raze();
    return meta_table.Init(db_.get(), kCurrentVersionNumber,
                           kCompatibleVersionNumber) &&
           CreateV1Schema(*db_);
  }

  DCHECK(sql::MetaTable::DoesTableExist(db_.get()));
  DCHECK(db_->DoesTableExist("interest_groups"));
  DCHECK(db_->DoesTableExist("join_history"));
  DCHECK(db_->DoesTableExist("bid_history"));
  DCHECK(db_->DoesTableExist("win_history"));

  // TODO(behamilton): handle migration.
  return true;
}

void InterestGroupStorage::JoinInterestGroup(const InterestGroupPtr group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;
  if (!DoJoinInterestGroup(*db_, group, base::Time::Now()))
    DLOG(ERROR) << "Could not join interest group: " << db_->GetErrorMessage();
}

void InterestGroupStorage::LeaveInterestGroup(const url::Origin& owner,
                                              const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;
  if (!DoLeaveInterestGroup(*db_, owner, name))
    DLOG(ERROR) << "Could not leave interest group: " << db_->GetErrorMessage();
}

void InterestGroupStorage::UpdateInterestGroup(const InterestGroupPtr group) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  if (!DoUpdateInterestGroup(*db_, group, base::Time::Now())) {
    DLOG(ERROR) << "Could not update interest group: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::RecordInterestGroupBid(const url::Origin& owner,
                                                  const std::string& name) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  if (!DoRecordInterestGroupBid(*db_, owner, name, base::Time::Now())) {
    DLOG(ERROR) << "Could not record win for interest group: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::RecordInterestGroupWin(const url::Origin& owner,
                                                  const std::string& name,
                                                  const std::string& ad_json) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  if (!DoRecordInterestGroupWin(*db_, owner, name, ad_json,
                                base::Time::Now())) {
    DLOG(ERROR) << "Could not record bid for interest group: "
                << db_->GetErrorMessage();
  }
}

std::vector<url::Origin> InterestGroupStorage::GetAllInterestGroupOwners() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return {};

  base::Optional<std::vector<url::Origin>> maybe_result =
      DoGetAllInterestGroupOwners(*db_, base::Time::Now());
  if (!maybe_result)
    return {};
  return std::move(maybe_result.value());
}

std::vector<BiddingInterestGroupPtr>
InterestGroupStorage::GetInterestGroupsForOwner(const url::Origin& owner) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return {};

  base::Optional<std::vector<BiddingInterestGroupPtr>> maybe_result =
      DoGetInterestGroupsForOwner(*db_, owner, base::Time::Now());
  if (!maybe_result)
    return {};
  base::UmaHistogramCounts1000("Storage.InterestGroup.PerSiteCount",
                               maybe_result->size());
  return std::move(maybe_result.value());
}

void InterestGroupStorage::DeleteInterestGroupData(
    const base::RepeatingCallback<bool(const url::Origin&)>& origin_matcher) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!EnsureDBInitialized())
    return;

  if (!DoDeleteInterestGroupData(*db_, origin_matcher)) {
    DLOG(ERROR) << "Could not delete interest group data: "
                << db_->GetErrorMessage();
  }
}

void InterestGroupStorage::PerformDBMaintenance() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // We can use base::Unretained(this) in the timer's closure because the timer
  // takes ownership of the callback we create here. The timer is guaranteed to
  // last the lifetime of this.
  base::Time now = base::Time::Now();
  if (now - last_access_time_ < kIdlePeriod) {
    // We're probably still in use. Let's try again in a bit.
    db_maintenance_timer_.Start(
        FROM_HERE, kIdlePeriod,
        base::BindOnce(&InterestGroupStorage::PerformDBMaintenance,
                       base::Unretained(this)));
    return;
  }

  // Schedule next run
  db_maintenance_timer_.Start(
      FROM_HERE, kMaintenanceInterval,
      base::BindOnce(&InterestGroupStorage::PerformDBMaintenance,
                     base::Unretained(this)));

  if (EnsureDBInitialized()) {
    DoPerformDatabaseMaintenance(*db_, now);
  }
}

std::vector<BiddingInterestGroupPtr>
InterestGroupStorage::GetAllInterestGroupsUnfilteredForTesting() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::Time infinite_past = base::Time::Min();
  std::vector<BiddingInterestGroupPtr> result;
  base::Optional<std::vector<url::Origin>> maybe_owners =
      DoGetAllInterestGroupOwners(*db_, infinite_past);
  if (!maybe_owners)
    return {};
  for (const auto& owner : *maybe_owners) {
    base::Optional<std::vector<BiddingInterestGroupPtr>> maybe_owner_results =
        DoGetInterestGroupsForOwner(*db_, owner, infinite_past);
    DCHECK(maybe_owner_results);
    std::move(maybe_owner_results->begin(), maybe_owner_results->end(),
              std::back_inserter(result));
  }
  return result;
}

void InterestGroupStorage::DatabaseErrorCallback(int extended_error,
                                                 sql::Statement* stmt) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (sql::IsErrorCatastrophic(extended_error)) {
    // Normally this will poison the database, causing any subsequent operations
    // to silently fail without any side effects. However, if RazeAndClose() is
    // called from the error callback in response to an error raised from within
    // sql::Database::Open, opening the now-razed database will be retried.
    db_->RazeAndClose();
    return;
  }

  // The default handling is to assert on debug and to ignore on release.
  if (!sql::Database::IsExpectedSqliteError(extended_error))
    DLOG(FATAL) << db_->GetErrorMessage();
}

}  // namespace content
