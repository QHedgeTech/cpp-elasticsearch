#include "elasticsearch.h"

#include <iostream>
#include <sstream>
#include <cstring>
#include <cassert>
#include <locale>
#include <vector>

ElasticSearch::ElasticSearch(const std::string& node, bool readOnly): _http(node, true), _readOnly(readOnly) {

    // Test if instance is active.
    if(!isActive())
        EXCEPTION("Cannot create engine, database is not active.");
}

ElasticSearch::~ElasticSearch() {
}

// Test connection with node.
bool ElasticSearch::isActive() {

    Json::Object root;

    try {
        _http.get(0, 0, &root);
    }
    catch(Exception& e){
        printf("get(0) failed in ElasticSearch::isActive(). Exception caught: %s\n", e.what());
        return false;
    }
    catch(std::exception& e){
        printf("get(0) failed in ElasticSearch::isActive(). std::exception caught: %s\n", e.what());
        return false;
    }
    catch(...){
        printf("get(0) failed in ElasticSearch::isActive().\n");
        return false;
    }

    if(root.empty())
        return false;

    if(!root.member("status") || root["status"].getInt() != 200){
        printf("Status is not 200. Cannot find Elasticsearch Node.\n");
        return false;
    }

    return true;
}

void ElasticSearch::readVersionString(){
    Json::Object msg;
    if (200 == _http.get("", 0, &msg)){
        if (msg.member("version") && msg["version"].getObject().member("number")){
            _versionString = msg["version"].getObject()["number"].getString();
        }
    }
}

int ElasticSearch::getMajorVersion(){
    return std::stoi(getVersionString());
}

const std::string& ElasticSearch::getVersionString(){
    if (_versionString.empty()) {
        readVersionString();
    }
    return _versionString;
}

// Request the document by index/_doc/id.
bool ElasticSearch::getDocument(const char* index, const char* id, Json::Object& msg){
    std::ostringstream oss;
    oss << index << "/_doc/" << id;
    _http.get(oss.str().c_str(), 0, &msg);
    return msg["found"];
}

// Request the document by index query key:value.
void ElasticSearch::getDocument(const std::string& index, const std::string& key, const std::string& value, Json::Object& msg){
    std::ostringstream oss;
    oss << index << "/_search";
    std::stringstream query;
    query << "{\"query\":{\"match\":{\""<< key << "\":\"" << value << "\"}}}";
    _http.post(oss.str().c_str(), query.str().c_str(), &msg);
}

/// Delete the document by index/_doc/id.
bool ElasticSearch::deleteDocument(const char* index, const char* id){
    if(_readOnly)
        return false;

    std::ostringstream oss;
    oss << index << "/_doc/" << id;
    Json::Object msg;
    _http.remove(oss.str().c_str(), 0, &msg);

    return msg.member("result") && msg.getValue("result").getString().compare("deleted")==0;
}

/// Delete the document by index
bool ElasticSearch::deleteAll(const char* index){
    if(_readOnly)
        return false;

    std::ostringstream uri, data;
    uri << index << "/_delete_by_query?conflicts=proceed";
    data << "{\"query\":{\"match_all\": {}}}";
    Json::Object msg;
    _http.post(uri.str().c_str(), data.str().c_str(), &msg);

    return (msg.member("total") && msg.member("deleted") && (msg["total"].getInt() - msg["deleted"].getInt())==0);
}

// Request the number of documents in index.
long unsigned int ElasticSearch::getDocumentCount(const char* index){
    std::ostringstream oss;
    oss << index << "/_count";
    Json::Object msg;
    _http.get(oss.str().c_str(),0,&msg);

    size_t pos = 0;
    if(msg.member("count"))
        pos = msg.getValue("count").getUnsignedInt();
    else
        printf("We did not find \"count\" member.\n");

    return pos;
}

// Test if document exists
bool ElasticSearch::exist(const std::string& index, const std::string& id){
    std::stringstream url;
    url << index << "/_source/" << id;

    Json::Object result;
    _http.head(url.str().c_str(), 0, &result);

    if(!result.member("found")){
        std::cout << result << std::endl;
        EXCEPTION("Database exception, field \"found\" must exist.");
    }

    return result.getValue("found");
}

/// Index a document.
bool ElasticSearch::index(const std::string& index, const std::string& id, const Json::Object& jData){

    if(_readOnly)
        return false;

    std::stringstream url;
    url << index << "/_doc/" << id;

    std::stringstream data;
    data << jData;

    Json::Object result;
    _http.put(url.str().c_str(), data.str().c_str(), &result);

    //ES2
    if (result.member("created") && result.getValue("created")) {
        return true;
    }

    //ES7
    if (result.member("result") && result.getValue("result").getString().compare("created")==0) {
        return true;
    }

    EXCEPTION("The index induces error.");

    std::cout << "endPoint: " << index << "/_doc/" << id << std::endl;
    std::cout << "jData" << jData.pretty() << std::endl;
    std::cout << "result" << result.pretty() << std::endl;

    EXCEPTION("The index returns ok: false.");
    return false;
}

/// Index a document with automatic id creation
std::string ElasticSearch::index(const std::string& index, const Json::Object& jData){

    if(_readOnly)
        return "";

    std::stringstream url;
    url << index << "/_doc/";

    std::stringstream data;
    data << jData;

    Json::Object result;
    _http.post(url.str().c_str(), data.str().c_str(), &result);

    if ((!result.member("created") || !result.getValue("created")) &&
        (!result.member("result") || result.getValue("result").getString().compare("created")!=0)) {
        std::cout << "url: " << url.str() << std::endl;
        std::cout << "data: " << data.str() << std::endl;
        std::cout << "result: " << result.str() << std::endl;
        EXCEPTION("The index induces error.");
    }

    return result.getValue("_id").getString();
}

// Update a document field.
bool ElasticSearch::update(const std::string& index, const std::string& id, const std::string& key, const std::string& value){
    if(_readOnly)
        return false;

    std::stringstream url;
    url << index << "/_update/" << id;

    std::stringstream data;
    data << "{\"doc\":{\"" << key << "\":\""<< value << "\"}}";

    Json::Object result;
    _http.post(url.str().c_str(), data.str().c_str(), &result);

    if(!result.member("_version"))
        EXCEPTION("The update failed.");

    return true;
}

// Update doccument fields.
bool ElasticSearch::update(const std::string& index, const std::string& id, const Json::Object& jData){
    if(_readOnly)
        return false;

    std::stringstream url;
    url << index << "/_update/" << id;

    std::stringstream data;
    data << "{\"doc\":" << jData;
    data << "}";

    Json::Object result;
    _http.post(url.str().c_str(), data.str().c_str(), &result);

    if(result.member("error"))
        EXCEPTION("The update doccument fields failed.");

    return true;
}

// Update or insert if the document does not already exists.
bool ElasticSearch::upsert(const std::string& index, const std::string& id, const Json::Object& jData){

    if(_readOnly)
        return false;

    std::stringstream url;
    url << index << "/_update/" << id;

    std::stringstream data;
    data << "{\"doc\":" << jData;
    data << ", \"doc_as_upsert\" : true}";

    Json::Object result;
    _http.post(url.str().c_str(), data.str().c_str(), &result);

    if(result.member("error"))
        EXCEPTION("The update doccument fields failed.");

    return true;
}

/// Search API of ES.
long ElasticSearch::search(const std::string& index, const std::string& query, Json::Object& result){

    std::stringstream url;
    url << index << "/_search";

    _http.post(url.str().c_str(), query.c_str(), &result);

    if(!result.member("timed_out")){
        std::cout << url.str() << " -d " << query << std::endl;
        std::cout << "result: " << result << std::endl;
        EXCEPTION("Search failed.");
    }

    if(result.getValue("timed_out")){
        std::cout << "result: " << result << std::endl;
        EXCEPTION("Search timed out.");
    }

    return result.getValue("hits").getObject().getValue("total").getObject().getValue("value").getLong();
}

// Test if index exists
bool ElasticSearch::exist(const std::string& index){
    return (200 == _http.head(index.c_str(), 0, 0));
}

// Create index, optionally with data (settings, mappings etc)
bool ElasticSearch::createIndex(const std::string& index, const char* data){
    return (200 == _http.put(index.c_str(), data, 0));
}

// Delete given index (and all documents, mappings)
bool ElasticSearch::deleteIndex(const std::string& index){
    return (200 == _http.remove(index.c_str(), 0, 0));
}

// Refresh the index.
void ElasticSearch::refresh(const std::string& index){
    std::ostringstream oss;
    oss << index << "/_refresh";

    Json::Object msg;
    _http.get(oss.str().c_str(), 0, &msg);
}

bool ElasticSearch::initScroll(std::string& scrollId, const std::string& index, const std::string& query, Json::Array& resultArray, int scrollSize) {
    std::ostringstream oss;
    oss << index << "/_search?scroll=1m&size=" << scrollSize;

    Json::Object msg;
    if (200 != _http.post(oss.str().c_str(), query.c_str(), &msg))
        return false;
    
    scrollId = msg["_scroll_id"].getString();

    appendHitsToArray(msg, resultArray);
    return true;
}

bool ElasticSearch::scrollNext(std::string& scrollId, Json::Array& resultArray) {
    Json::Object msg;

    if (getMajorVersion() < 7){
        if (200 != _http.post("/_search/scroll?scroll=1m", scrollId.c_str(), &msg))
            return false;
    }
    else
    {
        Json::Object body;
        body.addMemberByKey("scroll", "1m");
        body.addMemberByKey("scroll_id", scrollId);

        if (200 != _http.post("_search/scroll", body.str().c_str(), &msg))
            return false;
    }
    
    scrollId = msg["_scroll_id"].getString();
    
    appendHitsToArray(msg, resultArray);
    return true;
}

void ElasticSearch::clearScroll(const std::string& scrollId) {
    _http.remove("/_search/scroll", scrollId.c_str(), 0);
}

int ElasticSearch::fullScan(const std::string& index, const std::string& query, Json::Array& resultArray, int scrollSize) {
    resultArray.clear();
    
    std::string scrollId;
    if (!initScroll(scrollId, index, query, resultArray, scrollSize))
        return 0;

    size_t currentSize=resultArray.size(), newSize;
    while (scrollNext(scrollId, resultArray))
    {
        newSize = resultArray.size();
        if (currentSize == newSize)
            break;
        
        currentSize = newSize;
    }
    return currentSize;
}

void ElasticSearch::appendHitsToArray(const Json::Object& msg, Json::Array& resultArray) {

    if(!msg.member("hits"))
        EXCEPTION("Result corrupted, no member \"hits\".");

    if(!msg.getValue("hits").getObject().member("hits"))
        EXCEPTION("Result corrupted, no member \"hits\" nested in \"hits\".");

    for(const Json::Value& value : msg["hits"].getObject()["hits"].getArray()) {
        resultArray.addElement(value);
    }
}

// Bulk API of ES.
bool ElasticSearch::bulk(const char* data, Json::Object& jResult) {
	 if(_readOnly)
		return false;

	return (200 == _http.post("/_bulk", data, &jResult));
}

BulkBuilder::BulkBuilder() {}

void BulkBuilder::createCommand(const std::string &op, const std::string &index, const std::string &id = "") {
	Json::Object command;
	Json::Object commandParams;

	if (id != "") {
		commandParams.addMemberByKey("_id", id);
	}

	commandParams.addMemberByKey("_index", index);

	command.addMemberByKey(op, commandParams);
	operations.push_back(command);
}

void BulkBuilder::index(const std::string &index, const std::string &id, const Json::Object &fields) {
	createCommand("index", index, id);
	operations.push_back(fields);
}

void BulkBuilder::create(const std::string &index, const std::string &id, const Json::Object &fields) {
	createCommand("create", index, id);
	operations.push_back(fields);
}

void BulkBuilder::index(const std::string &index, const Json::Object &fields) {
	createCommand("index", index);
	operations.push_back(fields);
}

void BulkBuilder::create(const std::string &index, const Json::Object &fields) {
	createCommand("create", index);
	operations.push_back(fields);
}

void BulkBuilder::update(const std::string &index, const std::string &id, const Json::Object &body) {
    createCommand("update", index, id);
    operations.push_back(body);
}

void BulkBuilder::update_doc(const std::string &index, const std::string &id, const Json::Object &fields, bool upsert) {
	createCommand("update", index, id);

	Json::Object updateFields;
	updateFields.addMemberByKey("doc", fields);
    updateFields.addMemberByKey("doc_as_upsert", upsert);

	operations.push_back(updateFields);
}

void BulkBuilder::del(const std::string &index, const std::string &id) {
	createCommand("delete", index, id);
}

std::string BulkBuilder::str() {
	std::stringstream json;

	for(auto &operation : operations) {
		json << operation.str() << std::endl;
	}

	return json.str();
}

void BulkBuilder::clear() {
	operations.clear();
}

bool BulkBuilder::isEmpty() {
	return operations.empty();
}
