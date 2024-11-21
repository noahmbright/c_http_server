#! /bin/sh
kill -9 $(ps ax | grep tuke_http_server | awk '{print $1}')
