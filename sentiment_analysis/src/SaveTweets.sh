#!/bin/bash

#vsql -c "drop table tweets cascade;"

vsql -c "create table tweets (tweet_id numeric(32, 0) NOT NULL, query varchar(200), retweet_count int, created_at timestamptz, tweet varchar(500), user_id int, user_name varchar(20), location varchar(50), PRIMARY KEY(tweet_id, query, user_id));"

get_prop() 
{
    curl -v --get 'https://api.twitter.com/1.1/application/rate_limit_status.json ' --header 'Authorization: Bearer AAAAAAAAAAAAAAAAAAAAAOi%2FSQAAAAAAN%2BnUG6AJx7eufTLv5uV3nVxVOdw%3DekDelyGUn1cq1nHIii2uwHaCp73wPr69FL5Lil0' 2>&1 | grep -o 'search\/tweets":{[^}]*}' | sed 's/,/\n/g'| sed 's/}//g' | sed 's/search.*{//g' | sed 's/ //g' | grep $1 | awk -F ':' '{print $2}'
}

save_tweets()
{
    query=$1
    since_id=$2
    max_id=$3
    remaining=`get_prop remaining`
    reset=`get_prop reset`
    echo "Loading (\"$query\", "$since_id", "$max_id")..."
    echo "Remaining: $remaining"
    if [ $remaining -lt 50 ]; then 
        rest=$((reset - `date +%s`))
        for ((i=0; i<$rest; i++))
        do
            sleep 1
            echo -ne "Sleeping for $rest seconds...($i/$rest)\r"
        done
    fi
    vsql -c "insert into tweets select twitter_search('$query',$since_id,$max_id) over (); commit;"
    sleep 4
}


while true
do 
    # insert hot words from Google
    curl -v http://www.google.com/trends/hottrends/atom/hourly 2>&1 | grep '<li>' | sed 's/<[^>]\+>//g' | while read line; 
    do 
        vsql -q -t -c "insert into queries values('$line'); commit;"; 
    done
    # remove duplicates
    vsql -t -c "select distinct * into query_temp from queries; truncate table queries; insert into queries select * from query_temp; drop table query_temp; "; 
    vsql -t -c "select distinct tweet_id, query, retweet_count, created_at, tweet, user_id, user_name, location into tweets_temp from tweets;; truncate table tweets; insert into tweets select * from tweets_temp; drop table tweets_temp; "; 

    vsql -t -c "select query from queries order by random()" | while read query; 
    do
        if [ ${#query} -gt 0 ]; then
            max_id=`vsql -t -c "select max(tweet_id) from tweets where query='$query';"`
            if [ ${#max_id} -lt 10 ]; then
                max_id=0
            else
                max_id=$((max_id + 1))
            fi
            # load latest tweets
            save_tweets "$query" $max_id 0

            min_id=`vsql -t -c "select min(tweet_id) from tweets where query='$query';"`
            if [ ${#min_id} -lt 10 ]; then
                min_id=0
            else
                min_id=$((min_id - 1))
            fi
            # load older tweets
            save_tweets "$query" 0 $min_id 
        fi
    done
done
