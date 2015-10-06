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
#include <telldb/Field.hpp>

#include <boost/lexical_cast.hpp>
#include <stdexcept>


using namespace tell::store;

namespace tell {
namespace db {


bool Field::operator< (const Field& rhs) const {
    if (mType != rhs.mType) {
        throw std::invalid_argument("Can only compare fields of same type");
    }
    switch (mType) {
    case FieldType::NULLTYPE:
        return false;
    case FieldType::NOTYPE:
        throw std::invalid_argument("Can not compare fields without types");
    case FieldType::SMALLINT:
        return boost::any_cast<int16_t>(mValue) < boost::any_cast<int16_t>(rhs.mValue);
    case FieldType::INT:
        return boost::any_cast<int32_t>(mValue) < boost::any_cast<int32_t>(rhs.mValue);
    case FieldType::BIGINT:
        return boost::any_cast<int64_t>(mValue) < boost::any_cast<int64_t>(rhs.mValue);
    case FieldType::FLOAT:
        return boost::any_cast<float>(mValue) < boost::any_cast<float>(rhs.mValue);
    case FieldType::DOUBLE:
        return boost::any_cast<double>(mValue) < boost::any_cast<double>(rhs.mValue);
    case FieldType::TEXT:
        {
            const crossbow::string& l = *boost::any_cast<const crossbow::string*>(mValue);
            const crossbow::string& r = *boost::any_cast<const crossbow::string*>(rhs.mValue);
            return l < r;
        }
    case FieldType::BLOB:
        throw std::invalid_argument("Can not compare BLOBs");
    }
}

bool Field::operator<=(const Field& rhs) const {
    return *this < rhs || !(rhs < *this);
}

bool Field::operator>(const Field& rhs) const {
    return rhs < *this;
}

bool Field::operator>=(const Field& rhs) const {
    return rhs < *this || !(*this < rhs);
}

bool Field::operator==(const Field& rhs) const {
    return !(*this < rhs) && !(rhs < *this);
}

template<class T>
boost::any castTo(const T& value, FieldType target) {
    switch (target) {
    case FieldType::NULLTYPE:
    case FieldType::NOTYPE:
    case FieldType::BLOB:
        throw std::bad_cast();
    case FieldType::SMALLINT:
        return boost::lexical_cast<int16_t>(value);
    case FieldType::INT:
        return boost::lexical_cast<int32_t>(value);
    case FieldType::BIGINT:
        return boost::lexical_cast<int64_t>(value);
    case FieldType::FLOAT:
        return boost::lexical_cast<float>(value);
    case FieldType::DOUBLE:
        return boost::lexical_cast<double>(value);
    case FieldType::TEXT:
        return boost::lexical_cast<crossbow::string>(value);
    }
}

Field Field::cast(tell::store::FieldType t) {
    if (t == mType) return *this;
    switch (mType) {
    case FieldType::NULLTYPE:
    case FieldType::NOTYPE:
    case FieldType::BLOB:
        throw std::bad_cast();
    case FieldType::SMALLINT:
        return Field(t, castTo(boost::any_cast<int16_t>(mValue), t));
    case FieldType::INT:
        return Field(t, castTo(boost::any_cast<int32_t>(mValue), t));
    case FieldType::BIGINT:
        return Field(t, castTo(boost::any_cast<int64_t>(mValue), t));
    case FieldType::FLOAT:
        return Field(t, castTo(boost::any_cast<float>(mValue), t));
    case FieldType::DOUBLE:
        return Field(t, castTo(boost::any_cast<double>(mValue), t));
    case FieldType::TEXT:
        return Field(t, castTo(boost::any_cast<crossbow::string>(mValue), t));
    }
}

size_t Field::serialize(store::FieldType type, char* dest) const {
    switch (mType) {
    case FieldType::NULLTYPE:
        return 0;
    case FieldType::NOTYPE:
        throw std::invalid_argument("Can not serialize a notype");
    case FieldType::BLOB:
    case FieldType::TEXT:
        {
            const auto& v = boost::any_cast<const crossbow::string&>(mValue);
            int32_t sz = v.size();
            memcpy(dest, &sz, sizeof(sz));
            memcpy(dest + sizeof(sz), v.data(), v.size());
            auto res = sizeof(sz) + v.size();
            res += res % 8 == 0 ? 0 : (8 - (res % 8));
            return res;
        }
    case FieldType::SMALLINT:
        {
            int16_t v = boost::any_cast<int16_t>(mValue);
            memcpy(dest, &v, sizeof(v));
            return sizeof(v);
        }
    case FieldType::INT:
        {
            int32_t v = boost::any_cast<int32_t>(mValue);
            memcpy(dest, &v, sizeof(v));
            return sizeof(v);
        }
    case FieldType::BIGINT:
        {
            int64_t v = boost::any_cast<int64_t>(mValue);
            memcpy(dest, &v, sizeof(v));
            return sizeof(v);
        }
    case FieldType::FLOAT:
        {
            float v = boost::any_cast<float>(mValue);
            memcpy(dest, &v, sizeof(v));
            return sizeof(v);
        }
    case FieldType::DOUBLE:
        {
            double v = boost::any_cast<double>(mValue);
            memcpy(dest, &v, sizeof(v));
            return sizeof(v);
        }
    }
}

} // namespace db
} // namespace tell
