#!/bin/bash

while true
do 
    sleep 600
    query="zimmerman"
    echo "Loading \"$query\" ..."
    vsql -c "insert into tweets select twitter_search('$query') over ();"
    sleep 600

    query="arsenal"
    echo "Loading \"$query\" ..."
    vsql -c "insert into tweets select twitter_search('$query') over ();"
    sleep 900

    query="royal baby"
    echo "Loading \"$query\" ..."
    vsql -c "insert into tweets select twitter_search('$query') over ();"
    sleep 900

    query="vertica"
    echo "Loading \"$query\" ..."
    vsql -c "insert into tweets select twitter_search('$query') over ();"
    sleep 600

    query="soccer"
    echo "Loading \"$query\" ..."
    vsql -c "insert into tweets select twitter_search('$query') over ();"
    sleep 600

    query="castro"
    echo "Loading \"$query\" ..."
    vsql -c "insert into tweets select twitter_search('$query') over ();"
    sleep 600

    query="snowden"
    echo "Loading \"$query\" ..."
    vsql -c "insert into tweets select twitter_search('$query') over ();"
    sleep 600
done
