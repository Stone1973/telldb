/*
 * (C) Copyright 2015 ETH Zurich Systems Group (http://www.systems.ethz.ch/) and others.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * Contributors:
 *     Markus Pilman <mpilman@inf.ethz.ch>
 *     Simon Loesing <sloesing@inf.ethz.ch>
 *     Thomas Etter <etterth@gmail.com>
 *     Kevin Bocksrocker <kevin.bocksrocker@gmail.com>
 *     Lucas Braun <braunl@inf.ethz.ch>
 */
#include "TransactionCache.hpp"

#include <telldb/TellDB.hpp>
#include <telldb/Transaction.hpp>
#include <telldb/Exceptions.hpp>
#include <tellstore/ClientManager.hpp>

using namespace tell::store;

namespace tell {
namespace db {
using namespace impl;

Transaction::Transaction(ClientHandle& handle, TellDBContext& context,
        std::unique_ptr<commitmanager::SnapshotDescriptor> snapshot, TransactionType type)
    : mHandle(handle)
    , mContext(context)
    , mSnapshot(std::move(snapshot))
    , mCache(new (&mPool) TransactionCache(context, mHandle, *mSnapshot, mPool))
    , mType(type)
{
}

crossbow::ChunkMemoryPool& Transaction::pool() {
    return mPool;
}

Transaction::~Transaction() {
    if (!mCommitted) {
        rollback();
    }
}

Future<table_t> Transaction::openTable(const crossbow::string& name) {
    return mCache->openTable(name);
}

table_t Transaction::createTable(const crossbow::string& name, const store::Schema& schema) {
    return mCache->createTable(name, schema);
}

Future<Tuple> Transaction::get(table_t table, key_t key) {
    return mCache->get(table, key);
}

Iterator Transaction::lower_bound(table_t tableId, const crossbow::string& idxName, const KeyType& key) {
    return mCache->lower_bound(tableId, idxName, key);
}

Iterator Transaction::reverse_lower_bound(table_t tableId, const crossbow::string& idxName, const KeyType& key) {
    return mCache->reverse_lower_bound(tableId, idxName, key);
}

void Transaction::insert(table_t table, key_t key, const Tuple& tuple) {
    return mCache->insert(table, key, tuple);
}

void Transaction::update(table_t table, key_t key, const Tuple& from, const Tuple& to) {
    return mCache->update(table, key, from, to);
}

void Transaction::remove(table_t table, key_t key, const Tuple& tuple) {
    return mCache->remove(table, key, tuple);
}

void Transaction::commit() {
    writeBack();
    // if this succeeds, we can write back the indexes
    mHandle.commit(*mSnapshot);
    mCommitted = true;
}

void Transaction::rollback() {
    if (mCommitted) {
        throw std::logic_error("Transaction has already committed");
    }
    mCache->rollback();
    mHandle.commit(*mSnapshot);
    mCommitted = true;
}

void Transaction::writeUndoLog(std::pair<size_t, uint8_t*> log) {
    auto resp = mHandle.insert(mContext.clientTable->txTable(), mSnapshot->version(), 0, {
            std::make_pair("value", crossbow::string(reinterpret_cast<char*>(log.second), log.first))
    });
    __attribute__((unused)) auto res = resp->waitForResult();
    LOG_ASSERT(res, "Writeback did not succeed");
}

void Transaction::writeBack(bool withIndexes) {
    if (mCommitted) {
        throw std::logic_error("Transaction has already committed");
    }
    auto undoLog = mCache->undoLog(withIndexes);
    if (undoLog.first != 0 && mType != store::TransactionType::READ_WRITE) {
        throw std::logic_error("Transaction is read only");
    }
    writeUndoLog(undoLog);
    mCache->writeBack();
    if (withIndexes) {
        mCache->writeIndexes();
    }
}

const store::Record& Transaction::getRecord(table_t table) const {
    return mCache->record(table);
}

} // namespace db
} // namespace tell

