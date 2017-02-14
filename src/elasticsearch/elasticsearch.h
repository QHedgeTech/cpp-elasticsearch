#ifndef ELASTICSEARCH_H
#define ELASTICSEARCH_H

#include <string>
#include <sstream>
#include <list>
#include <mutex>
#include <vector>

#include "http/http.h"
#include "json/json.h"

/// API class for elastic search server.
/// Node: Instance of elastic search on server represented by url:port
class ElasticSearch {
    public:
        ElasticSearch(const std::string& node, bool readOnly = false);
        ~ElasticSearch();

         /// Test connection with node.
        bool isActive();

        /// Request document number of type T in index I.
        long unsigned int getDocumentCount(const char* index, const char* type);

        /// Request the document by index/type/id.
        bool getDocument(const char* index, const char* type, const char* id, Json::Object& msg);

        /// Request the document by index/type/ query key:value.
        void getDocument(const std::string& index, const std::string& type, const std::string& key, const std::string& value, Json::Object& msg);

        /// Delete the document by index/type/id.
        bool deleteDocument(const char* index, const char* type, const char* id);

        /// Delete the document by index/type.
        bool deleteAll(const char* index, const char* type);

		/// Test if document exists
        bool exist(const std::string& index, const std::string& type, const std::string& id);

        /// Get Id of document
        bool getId(const std::string& index, const std::string& type, const std::string& key, const std::string& value, std::string& id);

        /// Index a document.
        bool index(const std::string& index, const std::string& type, const std::string& id, const Json::Object& jData);

        /// Index a document with automatic id creation
        std::string index(const std::string& index, const std::string& type, const Json::Object& jData);

        /// Update a document field.
        bool update(const std::string& index, const std::string& type, const std::string& id, const std::string& key, const std::string& value);

        /// Update doccument fields.
        bool update(const std::string& index, const std::string& type, const std::string& id, const Json::Object& jData);

        /// Update or insert if the document does not already exists.
        bool upsert(const std::string& index, const std::string& type, const std::string& id, const Json::Object& jData);

        /// Search API of ES. Specify the doc type.
        long search(const std::string& index, const std::string& type, const std::string& query, Json::Object& result);

        // Bulk API
        bool bulk(const char*, Json::Object& jResult);

    public:
        /// Delete given type (and all documents, mappings)
        bool deleteType(const std::string& index, const std::string& type);

    public:
        /// Test if index exists
        bool exist(const std::string& index);

        /// Create index, optionally with data (settings, mappings etc)
        bool createIndex(const std::string& index, const char* data = NULL);
        
        /// Delete given index (and all types, documents, mappings)
        bool deleteIndex(const std::string& index);
        
        /// Refresh the index.
        void refresh(const std::string& index);

    public:
        /// Initialize a scroll search. Use the returned scroll id when calling scrollNext. Size is based on shardSize. Returns false on error
        bool initScroll(std::string& scrollId, const std::string& index, const std::string& type, const std::string& query, int scrollSize = 1000);

        /// Scroll to next matches of an initialized scroll search. scroll_id may be updated. End is reached when resultArray.empty() is true (in which scroll is automatically cleared). Returns false on error.
        bool scrollNext(std::string& scrollId, Json::Array& resultArray);

        /// Clear an initialized scroll search prior to its automatically 1 minute timeout
        void clearScroll(const std::string& scrollId);

        /// Perform a scan to get all results from a query.
        int fullScan(const std::string& index, const std::string& type, const std::string& query, Json::Array& resultArray, int scrollSize = 1000);

    private:
        void appendHitsToArray(const Json::Object& msg, Json::Array& resultArray);

    private:
        /// Private constructor.
        ElasticSearch();

        /// HTTP Connexion module.
        HTTP _http;

        /// Read Only option, all index functions return false.
        bool _readOnly;
};

class BulkBuilder {
	private:
		std::vector<Json::Object> operations;

		void createCommand(const std::string &op, const std::string &index, const std::string &type, const std::string &id);

	public:
		BulkBuilder();
		void index(const std::string &index, const std::string &type, const std::string &id, const Json::Object &fields);
		void create(const std::string &index, const std::string &type, const std::string &id, const Json::Object &fields);
		void index(const std::string &index, const std::string &type, const Json::Object &fields);
		void create(const std::string &index, const std::string &type, const Json::Object &fields);
		void update(const std::string &index, const std::string &type, const std::string &id, const Json::Object &body);
        void update_doc(const std::string &index, const std::string &type, const std::string &id, const Json::Object &fields, bool update = false);
		void del(const std::string &index, const std::string &type, const std::string &id);
		void upsert(const std::string &index, const std::string &type, const std::string &id, const Json::Object &fields);
		void clear();
		std::string str();
		bool isEmpty();
};

#endif // ELASTICSEARCH_H
