#include "BdTreeBackend.hpp"
#include <bdtree/error_code.h>

#include <stdexcept>
#include <utility>

namespace tell {
namespace db {
namespace {

const crossbow::string gPointerFieldName = "pptr";

store::GenericTuple createPtrTuple(bdtree::physical_pointer pptr) {
    return store::GenericTuple({
        std::make_pair(gPointerFieldName, static_cast<int64_t>(pptr.value))
    });
}

const crossbow::string gNodeFieldName = "node";

store::GenericTuple createNodeTuple(const char* data, size_t length) {
    return store::GenericTuple({
        std::make_pair(gNodeFieldName, crossbow::string(data, data + length))
    });
}

} // anonymous namespace

BdTreeNodeData::BdTreeNodeData(store::Table& table, store::Record::id_t id, std::unique_ptr<store::Tuple> tuple)
        : mTuple(std::move(tuple)),
          mSize(0x0u),
          mData(nullptr) {
    bool isNull;
    store::FieldType type;
    auto field = table.record().data(mTuple->data(), id, isNull, &type);
    if (isNull || type != store::FieldType::BLOB) {
        throw std::logic_error("Invalid field");
    }

    mSize = *reinterpret_cast<const uint32_t*>(field);
    mData = field + sizeof(uint32_t);
}

std::unique_ptr<store::Tuple> BdTreeBaseTable::doRead(uint64_t key, std::error_code& ec) {
    auto getFuture = mHandle.get(mTable.table(), key);
    if (!getFuture->waitForResult()) {
        ec = getFuture->error();
        return {nullptr};
    }
    auto tuple = getFuture->get();

    if (!tuple->found()) {
        ec = make_error_code(bdtree::error::object_doesnt_exist);
        return {nullptr};
    }

    return tuple;
}

bool BdTreeBaseTable::doInsert(uint64_t key, store::GenericTuple tuple, std::error_code& ec) {
    auto insertFuture = mHandle.insert(mTable.table(), key, 0x0u, std::move(tuple), true);
    if (!insertFuture->waitForResult()) {
        ec = insertFuture->error();
        return false;
    }
    if (!insertFuture->get()) {
        ec = make_error_code(bdtree::error::object_exists);
        return false;
    }

    return true;
}

bool BdTreeBaseTable::doUpdate(uint64_t key, store::GenericTuple tuple, uint64_t version, std::error_code& ec) {
    auto updateFuture = mHandle.update(mTable.table(), key, version, std::move(tuple));
    if (!updateFuture->waitForResult()) {
        ec = updateFuture->error();
        return false;
    }
    if (!updateFuture->get()) {
        ec = make_error_code(bdtree::error::wrong_version);
        return false;
    }

    return true;
}

bool BdTreeBaseTable::doRemove(uint64_t key, uint64_t version, std::error_code& ec) {
    auto removeFuture = mHandle.remove(mTable.table(), key, version);
    if (!removeFuture->waitForResult()) {
        ec = removeFuture->error();
        return false;
    }
    if (!removeFuture->get()) {
        ec = make_error_code(bdtree::error::wrong_version);
        return false;
    }

    return true;
}

store::Table BdTreePointerTable::createTable(store::ClientHandle& handle, const crossbow::string& name) {
    store::Schema schema(store::TableType::NON_TRANSACTIONAL);
    schema.addField(store::FieldType::BIGINT, gPointerFieldName, true);

    return handle.createTable(name, std::move(schema));
}

std::tuple<bdtree::physical_pointer, uint64_t> BdTreePointerTable::read(bdtree::logical_pointer lptr,
        std::error_code& ec) {
    auto tuple = doRead(lptr.value, ec);
    if (!tuple)
        return std::make_tuple(bdtree::physical_pointer{0x0u}, 0x0u);

    auto pptr = mTable.table().field<int64_t>(gPointerFieldName, tuple->data());
    return std::make_tuple(bdtree::physical_pointer{static_cast<uint64_t>(pptr)}, tuple->version());
}

uint64_t BdTreePointerTable::insert(bdtree::logical_pointer lptr, bdtree::physical_pointer pptr, std::error_code& ec) {
    return (doInsert(lptr.value, createPtrTuple(pptr), ec) ? 0x1u : 0x0u);
}

uint64_t BdTreePointerTable::update(bdtree::logical_pointer lptr, bdtree::physical_pointer pptr, uint64_t version,
        std::error_code& ec) {
    return (doUpdate(lptr.value, createPtrTuple(pptr), version, ec) ? version + 1 : 0x0u);
}

void BdTreePointerTable::remove(bdtree::logical_pointer lptr, uint64_t version, std::error_code& ec) {
    // In the case we have no version the bdtree tries to erase with version max
    // This is invalid in the TellStore as version max denotes the active version
    if (version == std::numeric_limits<uint64_t>::max()) {
        version = std::numeric_limits<uint64_t>::max() - 2;
    }
    doRemove(lptr.value, version, ec);
}

store::Table BdTreeNodeTable::createTable(store::ClientHandle& handle, const crossbow::string& name) {
    store::Schema schema(store::TableType::NON_TRANSACTIONAL);
    schema.addField(store::FieldType::BLOB, gNodeFieldName, true);

    return handle.createTable(name, std::move(schema));
}

BdTreeNodeTable::BdTreeNodeTable(store::ClientHandle& handle, TableData& table)
        : BdTreeBaseTable(handle, table) {
    if (!mTable.table().record().idOf(gNodeFieldName, mNodeDataId)) {
        throw std::logic_error("Node field not found");
    }
}

BdTreeNodeData BdTreeNodeTable::read(bdtree::physical_pointer pptr, std::error_code& ec) {
    auto tuple = doRead(pptr.value, ec);
    if (!tuple)
        return BdTreeNodeData();

    return BdTreeNodeData(mTable.table(), mNodeDataId, std::move(tuple));
}

void BdTreeNodeTable::insert(bdtree::physical_pointer pptr, const char* data, size_t length, std::error_code& ec) {
    doInsert(pptr.value, createNodeTuple(data, length), ec);
}

void BdTreeNodeTable::remove(bdtree::physical_pointer pptr, std::error_code& ec) {
    doRemove(pptr.value, 0x1u, ec);
}

} // namespace db
} // namespace tell
