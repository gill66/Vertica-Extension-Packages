/* Copyright (c) 2005 - 2011 Vertica, an HP company -*- C++ -*- */
/* 
 *
 * Description: Functions that help with full-text search over documents
 *
 * Create Date: Nov 25, 2011
 */
#include <curl/curl.h>

#include <sstream>
#include <set>
#include "Vertica.h"
#include "libstemmer.h"
#include <json/json.h>
#include "Words.h"
 
using namespace Vertica;
using namespace std;

// Make it a little harder to try and get a std::string from a NULL VString
// (which in the 5.0 API is a recipe for disaster -- aka crashes)
static inline const std::string getStringSafe(const VString &vstr) 
{
    return (vstr.isNull() || vstr.length() == 0)  ? "" : vstr.str();
}


/*
 * This is a simple function that stems an input word
 */
class Stem : public ScalarFunction
{
    sb_stemmer *stemmer;

public:
    // Set up the stemmer
    virtual void setup(ServerInterface &srvInterface, const SizedColumnTypes &argTypes) 
    {
        stemmer = sb_stemmer_new("english", NULL);
    }

    // Destroy the stemmer
    virtual void destroy(ServerInterface &srvInterface, const SizedColumnTypes &argTypes) 
    {
        sb_stemmer_delete(stemmer);
    }

    /*
     * This method processes a block of rows in a single invocation.
     *
     * The inputs are retrieved via arg_reader
     * The outputs are returned via arg_writer
     */
    virtual void processBlock(ServerInterface &srvInterface,
                              BlockReader &arg_reader,
                              BlockWriter &res_writer)
    {
        // Basic error checking
        if (arg_reader.getNumCols() != 1)
            vt_report_error(0, "Function only accept 1 arguments, but %zu provided", 
                            arg_reader.getNumCols());

        // While we have inputs to process
        do {
            // Get a copy of the input string
            std::string  inStr = getStringSafe(arg_reader.getStringRef(0));

            const unsigned char* stemword = sb_stemmer_stem(stemmer, reinterpret_cast<const sb_symbol*>(inStr.c_str()), inStr.size());

            // Copy string into results
            res_writer.getStringRef().copy(reinterpret_cast<const char*>(stemword));
            res_writer.next();
        } while (arg_reader.next());
    }
};

class StemFactory : public ScalarFunctionFactory
{
    // return an instance of RemoveSpace to perform the actual addition.
    virtual ScalarFunction *createScalarFunction(ServerInterface &interface)
    { return vt_createFuncObj(interface.allocator, Stem); }

    virtual void getPrototype(ServerInterface &interface,
                              ColumnTypes &argTypes,
                              ColumnTypes &returnType)
    {
        argTypes.addVarchar();
        returnType.addVarchar();
    }

    virtual void getReturnType(ServerInterface &srvInterface,
                               const SizedColumnTypes &argTypes,
                               SizedColumnTypes &returnType)
    {
        const VerticaType &t = argTypes.getColumnType(0);
        returnType.addVarchar(t.getStringLength());
    }
};

RegisterFactory(StemFactory);

static size_t
WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    stringstream *mem = (stringstream *)userp;

    mem->write(static_cast<const char*>(contents), realsize);
 
    return realsize;
}

static string fetch_url(CURL *curl_handle, const char *url)
{
    stringstream chunk;

    /* specify URL to get */ 
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);
 
    /* send all data to this function  */ 
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
 
    /* we pass our 'chunk' struct to the callback function */ 
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);
 
    /* some servers don't like requests that are made without a user-agent
       field, so we provide one */ 
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    /* add header options */
    struct curl_slist *slist=NULL;
    slist = curl_slist_append(slist, "Authorization: Bearer AAAAAAAAAAAAAAAAAAAAAOi%2FSQAAAAAAN%2BnUG6AJx7eufTLv5uV3nVxVOdw%3DekDelyGUn1cq1nHIii2uwHaCp73wPr69FL5Lil0");
    curl_easy_setopt(curl_handle, CURLOPT_HTTPHEADER, slist);
 
    /* get it! */ 
    curl_easy_perform(curl_handle);

    /* free the list */
    curl_slist_free_all(slist); 
            
    return chunk.str();
}

/*
 * Fetch the given URL as a string
 */
class FetchURL : public ScalarFunction
{
    CURL *curl_handle;

public:
    virtual void setup(ServerInterface &srvInterface, const SizedColumnTypes &argTypes) 
    {
        curl_global_init(CURL_GLOBAL_ALL);
 
        /* init the curl session */ 
        curl_handle = curl_easy_init();
    }

    // Destroy the stemmer
    virtual void destroy(ServerInterface &srvInterface, const SizedColumnTypes &argTypes) 
    {
        /* cleanup curl stuff */ 
        curl_easy_cleanup(curl_handle);
        /* we're done with libcurl, so clean it up */ 
        curl_global_cleanup();
    }

    /*
     * This method processes a block of rows in a single invocation.
     *
     * The inputs are retrieved via arg_reader
     * The outputs are returned via arg_writer
     */
    virtual void processBlock(ServerInterface &srvInterface,
                              BlockReader &arg_reader,
                              BlockWriter &res_writer)
    {
        // Basic error checking
        if (arg_reader.getNumCols() != 1)
            vt_report_error(0, "Function only accept 1 arguments, but %zu provided", 
                            arg_reader.getNumCols());

        try {
            // While we have inputs to process
            do {
                // Get a copy of the input string
                std::string  inStr = getStringSafe(arg_reader.getStringRef(0));

                /* fetch contents of URL */ 
                string contents = fetch_url(curl_handle, inStr.c_str()).substr(0, 65000);
 
                // Copy string into results
                res_writer.getStringRef().copy(contents);
                res_writer.next();
            } while (arg_reader.next());
        } catch (std::exception &e) {
            vt_report_error(0, e.what());
        }
    }
};

class FetchURLFactory : public ScalarFunctionFactory
{
    // return an instance of RemoveSpace to perform the actual addition.
    virtual ScalarFunction *createScalarFunction(ServerInterface &interface)
    { return vt_createFuncObj(interface.allocator, FetchURL); }

    virtual void getPrototype(ServerInterface &interface,
                              ColumnTypes &argTypes,
                              ColumnTypes &returnType)
    {
        argTypes.addVarchar();
        returnType.addVarchar();
    }

    virtual void getReturnType(ServerInterface &srvInterface,
                               const SizedColumnTypes &argTypes,
                               SizedColumnTypes &returnType)
    {
        returnType.addVarchar(65000); // max varchar size
    }
};

RegisterFactory(FetchURLFactory);


/*
 * Parse the Twitter JSON results
 */
class TwitterJSONParser : public TransformFunction
{
    Json::Value root;   // will contains the root value after parsing.
    Json::Reader reader;

public:

    virtual void processPartition(ServerInterface &srvInterface,
                                  PartitionReader &input,
                                  PartitionWriter &output)
    {
        // Basic error checking
        if (input.getNumCols() != 1)
            vt_report_error(0, "Function only accept 1 arguments, but %zu provided", 
                            input.getNumCols());

        // While we have inputs to process
        do {
            // Get a copy of the input string
            string  inStr = getStringSafe(input.getStringRef(0));

            bool success = reader.parse(inStr, root, false);
            if (!success)
                vt_report_error(0, "Malformed JSON text");
                
            // Get the query string and results
            string query = root.get("query", "-").asString();
            Json::Value results = root.get("statuses",  Json::Value::null);

            for (uint i=0; i<results.size(); i++) {
                Json::Value result = results[i];
                string text = result.get("text", "").asString();

                // Copy string into results
                output.getStringRef(0).copy(query);
                output.getStringRef(1).copy(text);
                output.next();
            }
        } while (input.next());
    }
};

class TwitterJSONFactory : public TransformFunctionFactory
{
    // return an instance of the transform function
    virtual TransformFunction *createTransformFunction(ServerInterface &interface)
    { return vt_createFuncObj(interface.allocator, TwitterJSONParser); }

    virtual void getPrototype(ServerInterface &interface,
                              ColumnTypes &argTypes,
                              ColumnTypes &returnType)
    {
        argTypes.addVarchar();
        returnType.addVarchar();
        returnType.addVarchar();
    }

    virtual void getReturnType(ServerInterface &srvInterface,
                               const SizedColumnTypes &argTypes,
                               SizedColumnTypes &returnType)
    {
        returnType.addVarchar(100, "query"); // query string size <= 100
        returnType.addVarchar(300, "text"); // tweet size <= 300
    }
};

RegisterFactory(TwitterJSONFactory);


/*
 * Search Twitter and return results
 */
class TwitterSearch : public TransformFunction
{
    Json::Value root;   // will contains the root value after parsing.
    Json::Reader reader;
    CURL *curl_handle;

public:
    virtual void setup(ServerInterface &srvInterface, const SizedColumnTypes &argTypes) 
    {
        curl_global_init(CURL_GLOBAL_ALL);
 
        /* init the curl session */ 
        curl_handle = curl_easy_init();
    }

    virtual void destroy(ServerInterface &srvInterface, const SizedColumnTypes &argTypes) 
    {
        /* cleanup curl stuff */ 
        curl_easy_cleanup(curl_handle);
        /* we're done with libcurl, so clean it up */ 
        curl_global_cleanup();
    }

    virtual void processPartition(ServerInterface &srvInterface,
                                  PartitionReader &input,
                                  PartitionWriter &output)
    {
        // Basic error checking
        if (input.getNumCols() != 3)
            vt_report_error(0, "Function only accept 2 arguments, but %zu provided", 
                            input.getNumCols());

        try {
            // While we have inputs to process
            do {
                // Get a copy of the input string
                string  inStr = getStringSafe(input.getStringRef(0));
                uint64 since_id = *input.getNumericRef(1).words;
                uint64 max_id = *input.getNumericRef(2).words;

                if (since_id > max_id && since_id != 0 && max_id != 0)
                    vt_report_error(0, "since_id cannot be greater than max_id");

                // URL encode the search string
                char *sstr = curl_easy_escape(curl_handle, inStr.c_str(), 0);

                stringstream ss;
                ss << "https://api.twitter.com/1.1/search/tweets.json?q=" << sstr;
                if (since_id > 0)
                    ss << "&since_id=" << since_id;
                if (max_id > 0)
                    ss << "&max_id=" << max_id;
                string url = ss.str();

                while(true) {
                    string contents = fetch_url(curl_handle, url.c_str());

                    bool success = reader.parse(contents, root, false);
                    if (!success) {
                        srvInterface.log("Malformed JSON text: %s", contents.c_str());
                        break;
                    }
                    // Get the query string, next page and results
                    Json::Value search_metadata = root.get("search_metadata", Json::Value::null);
                    if (search_metadata == Json::Value::null)
                        break;
                    string query = search_metadata.get("query", "-").asString();

                    Json::Value results = root.get("statuses",  Json::Value::null);
                    if (results.size() == 0)
                        break;

                    for (uint i=0; i<results.size(); i++) {
                        Json::Value result = results[i];
                        string text = result.get("text", "").asString().substr(0, 500);
                        string created_str = result.get("created_at", "").asString();
                        int retweet_count = result.get("retweet_count", 0).asInt();
                        string id_str = result.get("id_str", 0).asString();
                        uint64 tweetid = std::strtoull(id_str.c_str(), NULL, 10);
                        Json::Value user = result.get("user", Json::Value::null); 
                        int userid = 0;
                        string username = "";
                        string location = "";
                        if (user != Json::Value::null) 
                        {
                            userid = user.get("id", 0).asInt(); 
                            username = user.get("screen_name", "").asString();
                            location = user.get("location", "").asString().substr(0, 50);
                        }
                        struct tm tmlol = {0};
                        strptime(created_str.c_str(), "%a %b %d %H:%M:%S %z %Y", &tmlol);
                        time_t created_at = mktime(&tmlol);

                        // Copy string into results
                        output.getNumericRef(0).copy(tweetid);
                        output.getStringRef(1).copy(inStr);
                        output.setInt(2, retweet_count);
                        output.setTimestampTz(3, getTimestampTzFromUnixTime(created_at));
                        output.getStringRef(4).copy(text);
                        output.setInt(5, userid);
                        output.getStringRef(6).copy(username);
                        output.getStringRef(7).copy(location);
                        output.next();
                    }

                    // Get the next batch of results
                    url = "https://api.twitter.com/1.1/search/tweets.json";
                    url += search_metadata.get("next_results", "").asString();
                }
                curl_free(sstr);
            } while (input.next());
        } catch (std::exception &e) {
            vt_report_error(0, e.what());
        }
    }
};

class TwitterSearchFactory : public TransformFunctionFactory
{
    // return an instance of the transform function
    virtual TransformFunction *createTransformFunction(ServerInterface &interface)
    { return vt_createFuncObj(interface.allocator, TwitterSearch); }

    virtual void getPrototype(ServerInterface &interface,
                              ColumnTypes &argTypes,
                              ColumnTypes &returnType)
    {
        argTypes.addVarchar();
        argTypes.addNumeric();
        argTypes.addNumeric();
        returnType.addNumeric();
        returnType.addVarchar();
        returnType.addInt();
        returnType.addTimestampTz();
        returnType.addVarchar();
        returnType.addInt();
        returnType.addVarchar();
        returnType.addVarchar();
    }

    virtual void getReturnType(ServerInterface &srvInterface,
                               const SizedColumnTypes &argTypes,
                               SizedColumnTypes &returnType)
    {
        const VerticaType &t = argTypes.getColumnType(0);
        returnType.addNumeric(20, 0, "tweet_id");
        returnType.addVarchar(t.getStringLength() + 50, "query"); // to account for URL encoding
        returnType.addInt("retweet_count");
        returnType.addTimestampTz(20, "created_at");
        returnType.addVarchar(500, "text"); // tweet size <= 500
        returnType.addInt("user_id");
        returnType.addVarchar(20, "user_name");
        returnType.addVarchar(50, "user_location");
    }
};

RegisterFactory(TwitterSearchFactory);

/*
 * This is a simple function that gives the sentiment score of a piece of
 * text. The score is simply the number of positive words minus the number of
 * negative words in the text.
 */
class Sentiment : public ScalarFunction
{
    set<string> positive;
    set<string> negative;

    sb_stemmer *stemmer;

    static inline void tokenize(const string &text, vector<string> &tokens)
    {
        istringstream ss(text);

        do {
            std::string buffer;
            ss >> buffer;

            if (!buffer.empty()) 
                tokens.push_back(buffer);
        } while (ss);
        
    }
public:
    // Set up the positive and negative word sets
    virtual void setup(ServerInterface &srvInterface, const SizedColumnTypes &argTypes) 
    {
        stemmer = sb_stemmer_new("english", NULL);

        int psize = sizeof(pwords)/sizeof(char*);
        int nsize = sizeof(nwords)/sizeof(char*);

        for (int i=0; i<psize; i++) {
            string stemword = reinterpret_cast<const char*>(sb_stemmer_stem(stemmer, reinterpret_cast<const sb_symbol*>(pwords[i]), strlen(pwords[i])));
            positive.insert(stemword);
        }

        for (int i=0; i<nsize; i++) {
            string stemword = reinterpret_cast<const char*>(sb_stemmer_stem(stemmer, reinterpret_cast<const sb_symbol*>(nwords[i]), strlen(nwords[i])));
            negative.insert(stemword);
        }
    }

    // Destroy the stemmer
    virtual void destroy(ServerInterface &srvInterface, const SizedColumnTypes &argTypes) 
    {
        sb_stemmer_delete(stemmer);
    }

    /*
     * This method processes a block of rows in a single invocation.
     *
     * The inputs are retrieved via arg_reader
     * The outputs are returned via arg_writer
     */
    virtual void processBlock(ServerInterface &srvInterface,
                              BlockReader &arg_reader,
                              BlockWriter &res_writer)
    {
        // Basic error checking
        if (arg_reader.getNumCols() != 1)
            vt_report_error(0, "Function only accept 1 arguments, but %zu provided", 
                            arg_reader.getNumCols());

        // While we have inputs to process
        do {
            // Get a copy of the input string
            std::string  inStr = getStringSafe(arg_reader.getStringRef(0));

            vector<string> words;
            tokenize(inStr, words);

            int score = 0;

            for (uint i=0; i<words.size(); i++) {
                string stemword = reinterpret_cast<const char*>(sb_stemmer_stem(stemmer, reinterpret_cast<const sb_symbol*>(words[i].c_str()), words[i].size()));

                if (positive.count(stemword))
                    score++;
                if (negative.count(stemword))
                    score--;
            }

            // Copy score into results
            res_writer.setInt(score);
            res_writer.next();
        } while (arg_reader.next());
    }
};

class SentimentFactory : public ScalarFunctionFactory
{
    // return an instance of RemoveSpace to perform the actual addition.
    virtual ScalarFunction *createScalarFunction(ServerInterface &interface)
    { return vt_createFuncObj(interface.allocator, Sentiment); }

    virtual void getPrototype(ServerInterface &interface,
                              ColumnTypes &argTypes,
                              ColumnTypes &returnType)
    {
        argTypes.addVarchar();
        returnType.addInt();
    }
};

RegisterFactory(SentimentFactory);


