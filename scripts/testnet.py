import os
import json
import subprocess

params = {
    'cleos_path': os.environ.get("CLEOS", "cleos"),
    'nodeos_url': os.environ.get("CYBERWAY_URL", "http://localhost:8888"),
}
cleosCmd = "{cleos_path} --url {nodeos_url} ".format(**params)

class args:
    symbol = "GOLOS"
    token_precision = 3
    vesting_precision = 6
    token = '%d,%s' % (token_precision, symbol)
    vesting = '%d,%s' % (vesting_precision, symbol)
    root_private = '5K463ynhZoCDDa4RDcr63cUwWLTnKqmdcoTKTHBjqoKfv4u5V7p'
    root_public = 'GLS8Znrtgwt8TfpmbVpTKvA2oB8Nqey625CLN8bCN3TEbgx86Dsvr'
    golos_private = '5KekbiEKS4bwNptEtSawUygRb5sQ33P6EUZ6c4k4rEyQg7sARqW'
    golos_public = 'GLS6Tvw3apAGeHCUTWpf9DY4xvUoKwmuDatW7GV8ygkuZ8F6y4Yor'
    wallet_password = 'PW5JV8aRcMtZ8645EyCcZn9YfocvMFv52FP4VEuJQBWEzEwvS1d3v'


def jsonArg(a):
    return " '" + json.dumps(a) + "' "

def _cleos(arguments, *, output=True):
    cmd = cleosCmd + arguments
    if output:
        print("cleos: " + cmd)
    (exception, traceback) = (None, None)
    try:
        return subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT, universal_newlines=True)
        #return subprocess.check_output(cmd, shell=True, universal_newlines=True)
    except subprocess.CalledProcessError as e:
        import sys
        (exception, traceback) = (e, sys.exc_info()[2])

    msg = str(exception) + ' with output:\n' + exception.output
    raise Exception(msg).with_traceback(traceback)

def cleos(arguments, *, keys=None):
    if keys:
        trx = _cleos(arguments + ' --skip-sign --dont-broadcast')
        for k in keys:
            trx = _cleos("sign '%s' --private-key %s 2>/dev/null" % (trx, k), output=False)
        return _cleos("push transaction -j --skip-sign '%s'" % trx, output=False)
    else:
        return _cleos(arguments)


def pushAction(code, action, actor, args, *, additional='', delay=None, expiration=None, providebw=None, keys=None):
    additional += ' --delay-sec %d' % delay if delay else ''
    additional += ' --expiration %d' % expiration if expiration else ''
    if type(providebw) == type([]):
        additional += ''.join(' --bandwidth-provider ' + provider for provider in providebw)
    else:
        additional += ' --bandwidth-provider ' + providebw if providebw else ''
    if type(actor) == type([]):
        additional += ''.join(' -p ' + a for a in actor)
    else:
        additional += ' -p ' + actor
    action = 'push action -j %s %s %s %s' % (code, action, jsonArg(args), additional)
    return json.loads(cleos(action, keys=keys))

def importRootKeys():
    cleos('wallet import --private-key %s' % args.root_private)
    cleos('wallet import --private-key %s' % args.golos_private)
    pass

def removeRootKeys():
    cleos('wallet remove_key --password %s %s' % (args.wallet_password, args.root_public))
    cleos('wallet remove_key --password %s %s' % (args.wallet_password, args.golos_public))
    pass

def unlockWallet():
    cleos('wallet unlock --password %s' % args.wallet_password)

def parseAuthority(auth):
    if type(auth) == type([]):
        return [parseAuthority(a) for a in auth]
    d = auth.split('@',2)
    if len(d) == 1:
        d.extend(['active'])
    return {'actor':d[0], 'permission':d[1]}

def createAction(contract, action, actors, args):
    data = cleos("convert pack_action_data {contract} {action} {args}".format(
            contract=contract, action=action, args=jsonArg(args))).rstrip()
    return {
        'account':contract,
        'name':action,
        'authorization':parseAuthority(actors if type(actors)==type([]) else [actors]),
        'data':data
    }

class Trx:
    def __init__(self, expiration=None):
        additional = '--skip-sign --dont-broadcast'
        if expiration:
            additional += ' --expiration {exp}'.format(exp=expiration)
        self.trx = pushAction('cyber', 'checkwin', 'cyber', {}, additional=additional)
        self.trx['actions'] = []

    def addAction(self, contract, action, actors, args):
        self.trx['actions'].append(createAction(contract, action, actors, args))

    def getTrx(self):
        return self.trx

# --------------------- EOSIO functions ---------------------------------------

def createAuthority(keys, accounts):
    keys.sort()
    keysList = []
    accountsList = []
    for k in keys:
        keysList.extend([{'weight':1,'key':k}])

    acc = []
    for a in accounts:
        d = a.split('@',2)
        if len(d) == 1:
            d.extend(['active'])
        acc.append(d)
    acc.sort()
    for d in acc:
        accountsList.extend([{'weight':1,'permission':{'actor':d[0],'permission':d[1]}}])
    return {'threshold': 1, 'keys': keysList, 'accounts': accountsList, 'waits':[]}

def createAccount(creator, account, key, *, providebw=None, keys=None):
    additional = '--bandwidth-provider %s' % providebw if providebw else ''
    cleos('create account %s %s %s %s' % (creator, account, key, additional), keys=keys)

def getAccount(account):
    cleos('get account %s' % account)

def updateAuth(account, permission, parent, keys, accounts):
    pushAction('cyber', 'updateauth', account, {
        'account': account,
        'permission': permission,
        'parent': parent,
        'auth': createAuthority(keys, accounts)
    })

def linkAuth(account, code, action, permission):
    cleos('set action permission %s %s %s %s -p %s'%(account, code, action, permission, account))

def transfer(sender, recipient, amount, memo="", *, providebw=None, keys=None):
    pushAction('cyber.token', 'transfer', sender, [sender, recipient, amount, memo], providebw=providebw, keys=keys)

# --------------------- GOLOS functions ---------------------------------------

def openVestingBalance(account, payer=None, *, providebw=None, keys=None):
    if payer == None:
        payer = account
    pushAction('finteh.vest', 'open', payer, [account, args.vesting, payer], providebw=providebw, keys=keys)

def openTokenBalance(account, payer=None, *, providebw=None, keys=None):
    if payer == None:
        payer = account
    pushAction('cyber.token', 'open', payer, [account, args.token, payer], providebw=providebw, keys=keys)

def issueToken(account, amount, memo="", *, providebw=None, keys=None):
    pushAction('cyber.token', 'issue', 'finteh', [account, amount, memo], providebw=providebw, keys=keys)

def buyVesting(account, amount):
    issueToken(account, amount)
    transfer(account, 'finteh.vest', amount)   # buy vesting

def registerWitness(witness, url=None):
    if url == None:
        url = 'http://%s.witnesses.golos.io' % witness
    pushAction('finteh.ctrl', 'regwitness', witness, {
        'domain': args.token,
        'witness': witness,
        'url': url
    })

def voteWitness(voter, witness):
    pushAction('finteh.ctrl', 'votewitness', voter, [voter, witness])

def createPost(author, permlink, category, header, body, *, beneficiaries=[], providebw=None, keys=None):
    return pushAction('finteh.pub', 'createmssg', author, {
            'message_id':{'author':author, 'permlink':permlink}, 
            'parent_id':{'author':"", 'permlink':category}, 
            'beneficiaries':beneficiaries,
            'tokenprop':0,
            'vestpayment':False,
            'headermssg':header,
            'bodymssg':body,
            'languagemssg':'ru',
            'tags':[],
            'jsonmetadata':''
        }, providebw=providebw, keys=keys)

def createComment(author, permlink, pauthor, ppermlink, header, body, *, beneficiaries=[], providebw=None, keys=None):
    return pushAction('finteh.pub', 'createmssg', author, {
            'message_id':{'author':author, 'permlink':permlink}, 
            'parent_id':{'author':pauthor, 'permlink':ppermlink}, 
            'beneficiaries':beneficiaries,
            'tokenprop':0,
            'vestpayment':False,
            'headermssg':header,
            'bodymssg':body,
            'languagemssg':'ru',
            'tags':[],
            'jsonmetadata':''
        }, providebw=providebw, keys=keys)

def upvotePost(voter, author, permlink, weight, *, providebw=None, keys=None):
    return pushAction('finteh.pub', 'upvote', voter, {'voter':voter, 'message_id':{'author':author, 'permlink':permlink}, 'weight':weight}, providebw=providebw, keys=keys)

def downvotePost(voter, author, permlink, weight, *, providebw=None, keys=None):
    return pushAction('finteh.pub', 'downvote', voter, {'voter':voter, 'message_id':{'author':author, 'permlink':permlink}, 'weight':weight}, providebw=providebw, keys=keys)

def unvotePost(voter, author, permlink, *, providebw=None, keys=None):
    return pushAction('finteh.pub', 'unvote', voter, [voter, author, permlink], providebw=providebw, keys=keys)

