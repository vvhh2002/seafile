#!/bin/bash

. ../common-conf.sh

testdir=${seafile_dir}/tests/basic
conf1=${testdir}/conf1
base_worktree='/tmp/test-c10k'
base_conf='/tmp/test-conf'
PWD=`pwd`
base_port='13579'
base_cport='24680'
base_url='http://127.0.0.1:8000'
repo_id=${1}

rm -rf $base_worktree
mkdir $base_worktree

rm -rf $base_conf
mkdir $base_conf

conf_prepare()
{
    port=`expr $base_port \+ $1`
    cport=`expr $base_cport \+ $1`
    conf="${base_conf}/$1"
    mkdir $conf

    id=`echo $1 | sha1sum | awk '{print $1}'`

    ccnet-init -c ${conf}/conf -n "test${1}" -P ${port} -Q ${cport}
}

start_ccnet()
{
    worktree="$base_worktree/$1"
    port=`expr $base_port \+ $1`
    conf="${base_conf}/$1/conf"

    ${ccnet} -c ${conf} -f - -D all &
}

start_seaf_daemon()
{
    worktree="$base_worktree/$1"
    port=`expr $base_port \+ $1`
    conf="${base_conf}/$1/conf"

    ${seaf_daemon} -c ${conf} -d ${conf}/seafile-data -l - -w $worktree &
}

start_test()
{
    # ping API2
    pong=`curl ${base_url}/api2/ping/`
    if [ -z ${pong} ] || [ ${pong} != '"pong"' ]
    then
        echo "Ping API2 Error"
        pkill ccnet
        exit 1
    fi

    # obtain token
    token_json=`curl -d 'username=gnehzuil@sohu.com&password=19851014' ${base_url}/api2/auth-token/`
    if [ -z "${token_json}" ]
    then
        echo "Get Token Error"
        exit 1
    fi

    # parse token
    # {"token": "24fd3c026886e3121b2ca630805ed425c272cb96"}
    token=`echo ${token_json} | awk -F: '{print $2}' | awk -F'"' '{print $2}'`

    # token ping again
    pong=`curl -H "Authorization: Token ${token}" ${base_url}/api2/auth/ping/`
    if [ -z ${pong} ] || [ ${pong} != '"pong"' ]
    then
        echo "Auth Ping API2 Error"
        exit 1
    fi

    worktree="$base_worktree/$1"
    conf="${base_conf}/$1/conf"
    ${PWD}/../../web/test-c10k.py ${conf} ${worktree}/c10k ${token} ${repo_id}
}

for i in {1..100}
do
    conf_prepare $i
done

for i in {1..100}
do
    start_ccnet $i
    sleep 1
done
for i in {1..100}
do
    start_seaf_daemon $i
done
for i in {1..100}
do
    start_test $i
done
