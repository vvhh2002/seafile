#!/usr/bin/python2

import os
import sys
import simplejson as json

os.environ["CCNET_CONF_DIR"]=sys.argv[1]
import ccnet
import seafile

from seaserv import CCNET_CONF_PATH
from seaserv import seafile_rpc
from seaserv import get_repos, get_repo, \
        get_default_seafile_worktree

from pysearpc import SearpcError

pool = ccnet.ClientPool(CCNET_CONF_PATH)
ccnet_rpc = ccnet.CcnetRpcClient(pool, req_pool=True)
seafile_rpc = seafile.RpcClient(pool, req_pool=True)
seafile_threaded_rpc = seafile.ThreadedRpcClient(pool)

wt = sys.argv[2]
token = sys.argv[3]
repo_id = sys.argv[4]
curl_cmd = "curl -H 'Authorization: Token " + token + "' -H 'Accept: " + \
    "application/json; indent=4' http://127.0.0.1:8000/api2/repos/" + \
    repo_id + "/download-info/"

repo_info = os.popen(curl_cmd).read()
tmp = json.loads(repo_info)
encrypted = tmp['encrypted']
clone_token = tmp['token']
relay_id = tmp['relay_id']
relay_addr = tmp['relay_addr']
relay_port = tmp['relay_port']
email = tmp['email']
repo_name = tmp['repo_name']

seafile_rpc.clone(repo_id, relay_id, repo_name.encode('utf-8'),
                  wt.encode('utf-8'), clone_token, encrypted, relay_addr, relay_port,
                  email)
