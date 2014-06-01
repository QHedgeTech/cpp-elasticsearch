/*
 * Licensed to cpp-elasticsearch under one or more contributor
 * license agreements. See the NOTICE file distributed with
 * this work for additional information regarding copyright
 * ownership. Elasticsearch licenses this file to you under
 * the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef JSON_H
#define JSON_H

#include <map>
#include <list>
#include <string>

/// Handmade Json parser. Goal: optimized for elasticsearch.
/// Must be fast (0 copy parser, etc.)

namespace Json {

/**
  Json Class to parse incoming elasticsearch messages:
  Messages come from the libcurl library as char*. To avoid copy, we use const char* too.
  Moreover, we decided to implement a non-destructive parser. So we use a const char* and copy only element we have to, directly into instances.
  Structural characters in JSON are {}[],:

  Key is a quoted sequence of chars, distinct from each other within the same object.
  Value can be:
    - Number: double precision floating-point format.
    - String: a quoted sequence of chars and \ for escape.
    - Boolean: true or false
    - Array: a sequence of values.
    - Object: a container of key-value pairs.
    - Empty: null

    Version. 0
    Parses the string on the go (splits key / value).
    Distinguishes values if they are object/array all others are string.
    Internal representation: string.
**/


/// JsonKey, use std::string.
typedef std::string Key;

class Object;
class Array;

/// JsonValue
class Value {
    public:
        enum ValueType{ objectType, arrayType, stringType, booleanType, numberType, nullType };

        Value();
        Value(const Value& val);
        ~Value();

        const char* showType() const;
        const char* read(const char* pStart, const char* pEnd);
        bool operator==(const Value& v) const;
        bool operator!=(const Value& other) const {
            return !operator==(other);
        }
        // Return the string value.
        const std::string& getString() const;
        // Automatic cast in string.
        operator const std::string&() const;

        void show() const;
        bool empty() const;
        unsigned int getUnsignedInt() const;

        // Return the boolean value.
        bool getBoolean() const;
        // Automatic cast in int.
        operator bool() const;

        // Return the int value.
        int getInt() const;
        // Automatic cast in int.
        operator int() const;

        long int getLong() const;

        // Return the double value.
        double getDouble() const;
        // Automatic cast in string.
        operator double() const;

        // Return the double value.
        float getFloat() const;
        // Automatic cast in string.
        operator float() const;

        ///  Return the object.
        const Object& getObject() const;
        const Array& getArray() const;

        /// Set this value as a boolean.
        void setBoolean(bool b);

        /// Set this value as String.
        void setString(const std::string& value);

        /// Set this value as Object.
        void setObject(const Json::Object& obj);

        /// Set this value as Array.
        void setArray(const Json::Array& array);

        /// Set this value as a double.
        void setDouble(double v);

        /// Set this value as an int.
        void setInt(unsigned int u);

        /// Set this value as an int.
        void setInt(int u);

        /// Set this value as an int.
        void setLong(long l);

        /// Give access to member for this operator.
        friend std::ostream& operator<<(std::ostream& os, const Value& value);

        /// Export data, don't use it for string value.
        const std::string& data() const {return _data;}

        /// Returns the data in Json Format. Convert the values into string with escaped characters.
        static std::string escapeJsonString(const std::string& input);

        /// Weak equality that can compare value of different types.
        static bool weakEquality(const Json::Value& a, const Json::Value& b);

        /// Test if is null
        inline bool isNull() const {return (_type == nullType);}
        inline bool isObject() const {return (_type == objectType);}
        inline bool isArray() const {return (_type == arrayType);}

        // Output Json Value in a pretty format.
        std::string pretty(int tab = 0) const;

    private:

        /** The data could be stored in a variant type.
        *  Instead, we interpret the data when we access it.
        *  This is an optimization because we don't read data several times.
        *  Available types:
        *       - std::string
        *       - Json::Object
        *       - Json::Array
        *       - double
        *       - int
        *       - bool
        *       - long int
        *       - unsigned long int
        *       - empty
        **/

        // The complete data
        ValueType _type;
        std::string _data;
        Object* _object;
        Array* _array;
};

/// JsonObject
class Object {
    public:

        Object();
        Object(const Object& obj);

        /// Loops over the string and splits into members.
        const char* addMember(const char* startPtr, const char* endStr);

        /// Add member by key value.
        void addMemberByKey(const std::string& key, const std::string& str);
        void addMemberByKey(const std::string& key, const Json::Array& array);
        void addMemberByKey(const std::string& key, const Json::Object& obj);
        void addMemberByKey(const std::string& key, const Json::Value& value);
        void addMemberByKey(const std::string& key, double v);
        void addMemberByKey(const std::string& key, bool v);
        void addMemberByKey(const std::string& key, const char* s);
        void addMemberByKey(const std::string& key, unsigned int u);
        void addMemberByKey(const std::string& key, int i);
        void addMemberByKey(const std::string& key, long i);
        void addMemberByKey(const std::string& key, unsigned long i);

        /// Clear the map.
        void clear() { _memberMap.clear(); }

        /// Tells if the map is empty.
        bool empty() const { return _memberMap.empty();}

        /// Tells if the map is empty.
        size_t size() const { return _memberMap.size();}

        /// Tells if member exists.
        bool member(const std::string& key) const;

        /// Append another object to this one.
        void append(const Object& obj);

        /// Return the value of the member[key], key must exist in the map.
        const Value& getValue(const std::string& key) const;

        /// Equivalent to getValue. Return the value of the member[key], does not test if exists.
        const Value& operator[](const std::string& key) const noexcept;

        /// Give access to member for this operator.
        friend std::ostream& operator<<(std::ostream& os, const Object& obj);

        /// Returns the data in Json Format.
        std::string str() const;

        /// Output Json in a pretty format.
        std::string pretty(int tab = 0) const;

        /// o is in this object (each field exists and are equal)
        bool contain(const Object& o) const;

        bool operator==(const Object& v) const;
        bool operator!=(const Object& other) const {
            return !operator==(other);
        }

        class const_iterator {
          std::map< Key, Value >::const_iterator _it;
        public:
          const_iterator(const std::map< Key, Value >::const_iterator& it) : _it(it) {}
          const_iterator(const const_iterator& it) : _it(it._it) {}
          const_iterator& operator++() {
              ++_it;
              return *this;}
          const Key& operator*() const {return _it->first;}
          const Key& key() const {return _it->first;}
          const Value& value() const {return _it->second;}
          bool operator!=(const const_iterator& rhs) {return _it != rhs._it;}
        };

        const const_iterator begin() const { return const_iterator(_memberMap.cbegin()); }
        const const_iterator end() const { return const_iterator(_memberMap.cend()); }

    private:
        std::map< Key, Value > _memberMap;
};


/// JsonArray
class Array {
    public:
        Array();

        /// Loops over the string and splits into elements.
        const char* addElement(const char* startPtr, const char* endStr);

        /// Copy and add this value to the list.
        void addElement(const Json::Value& val);

        /// Copy the object to a value and add this value to the list.
        void addElement(const Json::Object& obj);

        /// Tells if the list is empty.
        size_t size() const { return _elementList.size();}

        /// Tells if the list is empty.
        void clear() { return _elementList.clear();}

        /// Tells if the list is empty.
        bool empty() const { return _elementList.empty();}

        /// Returns the first value of the list.
        const Value& first() const { return _elementList.front(); }

        bool operator==(const Array& v) const;
        bool operator!=(const Array& other) const {
            return !operator==(other);
        }

        /// Give access to member for this operator.
        friend std::ostream& operator<<(std::ostream& os, const Array& array);

        /// Returns the data in Json Format.
        std::string str() const;

        class const_iterator {
          std::list<Value>::const_iterator _it;
        public:
          const_iterator(const std::list<Value>::const_iterator& it) : _it(it) {}
          const_iterator(const const_iterator& it) : _it(it._it) {}
          const_iterator& operator++() {
              ++_it;
              return *this;
          }
          const Value& operator*() const { return *_it; }
          bool operator!=(const const_iterator& rhs) {return _it != rhs._it;}
        };

        const const_iterator begin() const { return const_iterator(_elementList.cbegin()); }
        const const_iterator end() const { return const_iterator(_elementList.cend()); }

        /// Output Json in a pretty format with same colors as Marvel/Sense.
        std::string pretty(int tab = 0) const;

    private:
        std::list<Value> _elementList;
};

}


#endif // JSON_H
