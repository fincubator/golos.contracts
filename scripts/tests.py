#!/usr/bin/python3

import unittest
import testnet
import subprocess
import json
import os
import re
import random
import time
import socket

golosIoKey='5JUKEmKs7sMX5fxv5PnHnShnUm4mmizfyBtWc8kgWnDocH9R6fr'
testingKey='5JdhhMMJdb1KEyCatAynRLruxVvi7mWPywiSjpLYqKqgsT4qjsN'
techKey   ='5KQwrPbwdL6PhXujxW37FSSQZ1JiwsST4cqQzDeyXtP79zkvFD3'

def wait(timeout):
    while timeout > 0:
        w = 3 if timeout > 3 else timeout
        print('\r                    \rWait %d sec' % timeout, flush=True, end='')
        timeout -= w
        time.sleep(w)
    print('\r                      \r', flush=True, end='')

def randomName():
    letters = 'abcdefghijklmnopqrstuvwxyz12345'
    return ''.join(random.choice(letters) for i in range(12))

def randomUsername():
    letters = 'abcdefghijklmnopqrstuvwxyz0123456789'
    return ''.join(random.choice(letters) for i in range(16))

def randomPermlink():
    letters = "abcdefghijklmnopqrstuvwxyz0123456789-"
    return ''.join(random.choice(letters) for i in range(128))

def randomText(length):
    letters = " ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789!@#$%^&*()_+-=;:,.<>/?\\~`"
    return ''.join(random.choice(letters) for i in range(length))

def setUpModule():
    print('setUpModule')
    info = json.loads(testnet.cleos('get info'))
    print(json.dumps(info, indent=3))

    testnet.cleos("wallet create --name test --to-console")
    for key in [techKey]:
        importPrivateKey(key)

def tearDownModule():
    print('tearDownModule')
    os.remove(os.environ['HOME']+'/eosio-wallet/test.wallet')

def createKey():
    data = testnet.cleos("create key --to-console")
    m = re.search('Private key: ([a-zA-Z0-9]+)\nPublic key: ([a-zA-Z0-9]+)', data)
    return (m.group(1), m.group(2))

def importPrivateKey(private):
    testnet.cleos("wallet import --name test --private-key %s" % private)

def createRandomAccount(key, *, creator='tech', providebw=None, keys=None):
    letters = "abcdefghijklmnopqrstuvwxyz12345"
    while True:
        name = ''.join(random.choice(letters) for i in range(12))
        try:
            getAccount(name)
        except:
            break
    testnet.createAccount(creator, name, key, providebw=providebw, keys=keys)
    return name

def getAccount(account):
    return json.loads(testnet.cleos('get account %s -j' % account))

def getResourceUsage(account):
    info = getAccount(account)
    return {
        'cpu': info['cpu_limit']['used'],
        'net': info['net_limit']['used'],
        'ram': info['ram_limit']['used'],
        'storage': info['storage_limit']['used']
    }

def resolve(username):
    data = testnet.cleos('resolve name %s' % username)
    m = re.match('"[a-z1-5.]+@[a-z1-5.]+" resolves to:\n  Domain:   ([a-z0-9.]+)\n  Username: ([a-z0-9.]+)\n', data)
    if m:
        return m.group(2)
    else:
        return None



class TestTestnet(unittest.TestCase):

    def test_createKey(self):
        (private,public) = createKey()
        self.assertEqual(private[:1], '5')
        self.assertEqual(public[:3], 'GLS')

    def test_createAccount(self):
        (private,public) = createKey()
        importPrivateKey(private)

        account = createRandomAccount(public)
        info = getAccount(account)
        self.assertEqual(info['account_name'], account)
        self.assertEqual(info['permissions'][0]['perm_name'], 'active')
        self.assertEqual(info['permissions'][0]['required_auth']['keys'][0]['key'], public)

    @unittest.skip("Incorrect check of consumed resources")
    def test_resourceUsage(self):
        # create user account
        (private,public) = createKey()
        user = createRandomAccount(public, creator='tech')
        importPrivateKey(private)

        # recuperate resource usage for `tech` account
        testnet.pushAction('cyber.token', 'open', 'tech', ['tech', '4,CYBER', 'tech'], additional='-f')

        # push test transaction
        userUsage = getResourceUsage(user)
        techUsage = getResourceUsage('tech')
        trx = testnet.pushAction('finteh.vest', 'open', 'tech', [user, '6,GOLOS', 'tech'])
        receipt = trx['processed']['receipt']
        self.assertLess(0, receipt['cpu_usage_us'])
        self.assertLess(0, receipt['net_usage_words'])
        self.assertLess(0, receipt['ram_kbytes'])
#        self.assertLess(0, receipt['storage_kbytes'])          # BUG: storage rounded down to 1kByte
        #print(json.dumps(trx, indent=3))

        # check that user doesn't pay for transaction
        self.assertEqual(userUsage, getResourceUsage(user))

        # check that `tech` account pay for transaction
        techUsage2 = getResourceUsage('tech')
        print("Was: %s, now: %s" % (techUsage, techUsage2))
        self.assertEqual(techUsage2['cpu'], techUsage['cpu'] + receipt['cpu_usage_us'])
        self.assertEqual(techUsage2['net'], techUsage['net'] + receipt['net_usage_words']*8)
        self.assertEqual(techUsage2['ram'], techUsage['ram'] + receipt['ram_kbytes']*1024)
        self.assertGreaterEqual(techUsage2['storage'], techUsage['storage'] +  receipt['storage_kbytes']   *1024)
        self.assertLessEqual   (techUsage2['storage'], techUsage['storage'] + (receipt['storage_kbytes']+1)*1024)

    @unittest.skip("Resource delegation required")
    def test_bandwidthProvider(self):
        # Check that resource for transactions was provided by provider account

        # Create user account
        (user_private,user_public) = createKey()
        importPrivateKey(user_private)
        user = createRandomAccount(user_public, creator='tech')

        # Create provider account
        (provider_private,provider_public) = createKey()
        importPrivateKey(provider_private)
        provider = createRandomAccount(provider_public, creator='tech')

        # recuperate resource usage for provider
        testnet.pushAction('cyber.token', 'open', provider, [provider, '4,CYBER', provider], additional='-f')

        # Get initial resource usage
        techUsage = getResourceUsage('tech')
        userUsage = getResourceUsage(user)
        providerUsage = getResourceUsage(provider)

        # Publish action with provider
        trx = testnet.pushAction('cyber.token', 'open', user, [user, '4,CYBER', user], providebw=user+'/'+provider)
        receipt = trx['processed']['receipt']

        # Check that `tech` and user resource usage doesn't changed
        self.assertEqual(techUsage, getResourceUsage('tech'))
        self.assertEqual(userUsage, getResourceUsage(user))

        # Check that provider resource usage increased
        providerUsage2 = getResourceUsage(provider)
        print("Was: %s, now: %s" % (providerUsage, providerUsage2))
#        self.assertEqual(providerUsage2['cpu'], providerUsage['cpu'] + receipt['cpu_usage_us'])          # STRANGE: sometimes value is one more than expected
#        self.assertEqual(providerUsage2['net'], providerUsage['net'] + receipt['net_usage_words']*8)     # STRANGE: sometimes value is one more than expected
        self.assertEqual(providerUsage2['ram'], providerUsage['ram'] + receipt['ram_kbytes']*1024)
        self.assertEqual(providerUsage2['storage'], providerUsage['storage'] + receipt['storage_kbytes']*1024)




class TestTransit(unittest.TestCase):
    def test_cyberAuthority(self):
        # Check that block producers has correct authority
        cyber = getAccount('cyber')
        permissions = {}
        for perm in cyber['permissions']:
            permissions[perm['perm_name']] = perm

        expected = {
            'active': {
                "perm_name": "active", "parent": "owner",
                "required_auth": {"threshold": 1, "keys": [], "accounts": [], "waits": []}
            },
            'owner': {
                "perm_name": "owner", "parent": "",
                "required_auth": {"threshold": 1, "keys": [], "accounts": [], "waits": []}
            },
            'prods': {
                "perm_name": "prods", "parent": "active",
                "required_auth": {
                    "threshold": 2,
                    "keys": [],
                    "accounts": [
                        {"permission": {"actor": "cyber.prods", "permission": "active"}, "weight": 1}
                    ],
                    "waits": [
                        {"wait_sec": 1209600, "weight": 1}
                    ]
                }
            }
        }
        self.assertEqual(permissions, expected)

    def test_golosAccounts(self):
        # Check new Golos accounts names
        account_list = (
            ('golos',     '1glrbzwsvdbq'),
            ('golosio',   'qwm3tgmbeog5'),
            ('goloscore', 'ggfpapchob2e'),
        )

        for (name, account) in account_list:
            self.assertEqual(account, resolve(name+'@golos'))

        gls = getAccount('finteh')
        permissions = {}
        for perm in gls['permissions']:
            permissions[perm['perm_name']] = perm
        self.assertEqual(permissions['golos.io'], {
            'perm_name': 'golos.io', 'parent': 'active',
            'required_auth': {'threshold': 1, 'keys': [], 'accounts': [
                {'permission': {'actor': 'qwm3tgmbeog5', 'permission': 'golos.io'}, 'weight': 1}
            ], 'waits': []}
        })

        golosio = getAccount('golosio@golos')
        permissions = {}
        for perm in golosio['permissions']:
            permissions[perm['perm_name']] = perm
        self.assertEqual(permissions['golos.io'], {
            'perm_name': 'golos.io', 'parent': 'active',
            'required_auth': {'threshold': 1, 'keys': [
                {'key': 'GLS76taNhqGMG4GptyJ2RYXgpJAH9zZzetyXW3troKFkpGciW8hjs', 'weight': 1}
            ], 'accounts': [], 'waits': []}
        })


class TestFreeUser(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (private, public) = createKey();
        importPrivateKey(private)
        user = createRandomAccount(public)
        testnet.pushAction('cyber.token', 'transfer', 'tech', ['tech', 'cyber.stake', '100.0000 CYBER', user])
        testnet.openVestingBalance(user, 'tech')
        cls.freeUser = user

    def test_createPost(self):
        permlink = randomPermlink()
        publishUsage = getResourceUsage('finteh.pub')

        # Check that user can post using own bandwidth
        testnet.createPost(self.freeUser, permlink, "", randomText(32), randomText(1024))
        print("Was: %s, current: %s" % (publishUsage, getResourceUsage('finteh.pub')))

        # Check that user can upvote using own bandwidth
        testnet.upvotePost(self.freeUser, self.freeUser, permlink, 10000)

class TestGolosIo(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        (private,public) = createKey()
        cls.siteKey = private
        cls.siteAccount = createRandomAccount(public)
        print("siteAccount: ", cls.siteAccount)

    def test_createUser(self):
        (private,public) = createKey()
        username = randomUsername()

        user = createRandomAccount(
                public, creator=self.siteAccount,
                providebw=self.siteAccount+'/finteh@providebw',
                keys=[self.siteKey, golosIoKey])

        testnet.pushAction(
                'finteh.vest', 'open', self.siteAccount, [user, testnet.args.vesting, self.siteAccount],
                providebw=self.siteAccount+'/finteh@providebw',
                keys=[self.siteKey, golosIoKey])

        testnet.pushAction(
                'cyber.token', 'open', self.siteAccount, [user, testnet.args.token, self.siteAccount],
                providebw=self.siteAccount+'/finteh@providebw',
                keys=[self.siteKey, golosIoKey])

        testnet.pushAction(
                'cyber.domain', 'newusername', 'finteh@createuser', ['finteh', user, username],
                keys=[golosIoKey])

    def test_createPost(self):
        (private,public) = createKey()
        user = createRandomAccount(public)
        testnet.pushAction('finteh.vest', 'open', 'tech', [user, testnet.args.vesting, 'tech'])

        permlink = randomPermlink()
        publishUsage = getResourceUsage('finteh.pub')
        testnet.createPost(
                user, permlink, "", randomText(32), randomText(1024),
                providebw=user+'/finteh@providebw',
                keys=[private, golosIoKey])
        print("Was: %s, current: %s" % (publishUsage, getResourceUsage('finteh.pub')))

        testnet.upvotePost(
                user, user, permlink, 10000,
                providebw=user+'/finteh@providebw',
                keys=[private, golosIoKey])

    def test_addReferral(self):
        params = json.loads(testnet.cleos('get table finteh.ref finteh.ref refparams'))['rows'][0]

        (private1,public1) = createKey()
        user1 = createRandomAccount(public1)

        (private2,public2) = createKey()
        user2 = createRandomAccount(public2)

        percent = params['percent_params']['max_percent']
        breakout = params['breakout_params']['min_breakout']
        expire = 60
        testnet.pushAction(
                'finteh.ref', 'addreferral', 'finteh.ref@newreferral',
                [user1, user2, percent, expire, breakout],
                providebw='finteh.ref/finteh@providebw',
                keys=[golosIoKey])

    def test_doesntAllowedChangeParams(self):
        # Check that golos.io can't change parameters without leaders
        params = json.loads(testnet.cleos('get table finteh.vest GOLOS vestparams'))['rows'][0]
        min_amount = params['min_amount']['min_amount']
        min_amount += -1 if min_amount%2 else +1

        with self.assertRaisesRegex(Exception, 'transaction declares authority \'{"actor":"finteh","permission":"active"}\', but does not have signatures for it'):
            testnet.pushAction(
                    'finteh.vest', 'setparams', 'finteh@active', ["6,GOLOS",[["vesting_amount",{"min_amount":min_amount}]]],
                    keys=[golosIoKey])

        with self.assertRaisesRegex(Exception, 'action declares irrelevant authority \'{"actor":"finteh","permission":"golos.io"}\'; minimum authority is {"actor":"finteh","permission":"active"}'):
            testnet.pushAction(
                    'finteh.vest', 'setparams', 'finteh@golos.io', ["6,GOLOS",[["vesting_amount",{"min_amount":min_amount}]]],
                    keys=[golosIoKey])

    def test_emitCallByAny(self):
        (private,public) = createKey()
        user = createRandomAccount(public)

        try:
            testnet.pushAction('finteh.emit', 'emit', user, [],
                    providebw=user+'/tech@active',
                    keys=[private,techKey])
        except Exception as err:
            self.assertRegex(str(err), 'assertion failure with message: emit called too early')

    def test_closemssgsCallByAny(self):
        (private,public) = createKey()
        user = createRandomAccount(public)

        testnet.pushAction('finteh.pub', 'closemssgs', user, [user],
                providebw=user+'/tech@active',
                keys=[private,techKey])

    def test_paymssgrwrdCallByAny(self):
        (private,public) = createKey()
        author = createRandomAccount(public)
        testnet.openVestingBalance(author, 'tech', providebw=author+'/tech', keys=[techKey])

        (private2,public2) = createKey()
        user = createRandomAccount(public2)

        permlink = randomPermlink()
        testnet.createPost(
                author, permlink, "", randomText(32), randomText(1024),
                providebw=author+'/tech@active',
                keys=[private, techKey])

        with self.assertRaisesRegex(Exception, "Message doesn't closed"):
            testnet.pushAction(
                    'finteh.pub', 'paymssgrwrd', user, {'message_id':{'author':author,'permlink':permlink}},
                    providebw=user+'/tech@active',
                    keys=[private2, techKey])

    def test_timeoutCallByAny(self):
        (private,public) = createKey()
        user = createRandomAccount(public)

        try:
            testnet.pushAction('finteh.vest', 'timeout', user, ["6,GOLOS"],
                    providebw=[user+'/tech@active', 'finteh.vest/tech@active'],
                    keys=[private, techKey])
            self.fail('Timeout does not scheduled')
        except Exception as err:
            self.assertRegex(str(err), 'deferred transaction with the same sender_id and payer already exists')

        testnet.pushAction('finteh.vest', 'timeoutconv', user, [],
                providebw=[user+'/tech@active', 'finteh.ctrl/tech@active'],
                keys=[private, techKey])

        testnet.pushAction('finteh.vest', 'timeoutrdel', user, [],
                providebw=user+'/tech@active',
                keys=[private, techKey])

    def test_closeoldrefCallByAny(self):
        (private,public) = createKey()
        user = createRandomAccount(public)

        testnet.pushAction('finteh.ref', 'closeoldref', user, [],
                providebw=user+'/tech@active',
                keys=[private, techKey])

    def test_delegateUndelegateVesting(self):
        (private1, public1) = createKey()
        user1 = createRandomAccount(public1)
        testnet.openVestingBalance(user1, 'tech')

        (private2, public2) = createKey()
        user2 = createRandomAccount(public2)
        testnet.openVestingBalance(user2, 'tech')

        amount = '100.000 GOLOS'
        testnet.issueToken(user1, amount, keys=[testingKey])
        testnet.transfer(user1, 'finteh.vest', amount, providebw=user1+'/finteh@providebw', keys=[golosIoKey, private1])

        user1Vesting = json.loads(testnet.cleos('get table finteh.vest %s accounts' % user1))['rows'][0]
        vestAmount = user1Vesting['vesting'].split(' ', 2)
        self.assertGreater(float(vestAmount[0]), 100111.000000)
        self.assertEqual(vestAmount[1], 'GOLOS')

        vestAmount = '100111.000000 GOLOS'
        testnet.pushAction('finteh.vest', 'delegate', [user1, user2],
            {'from':user1, 'to':user2, 'quantity':vestAmount, 'memo':'', 'interest_rate':1000},
            providebw=[user1+'/finteh@providebw', user2+'/finteh@providebw'],
            keys=[golosIoKey, private1, private2])

        user1Vesting = json.loads(testnet.cleos('get table finteh.vest %s accounts' % user1))['rows'][0]
        user2Vesting = json.loads(testnet.cleos('get table finteh.vest %s accounts' % user2))['rows'][0]
        self.assertEqual(user1Vesting['delegated'], '100111.000000 GOLOS')
        self.assertEqual(user2Vesting['received'],  '100111.000000 GOLOS')

        wait(0)    # get from params `delegation.min_time`

        testnet.pushAction('finteh.vest', 'undelegate', user1,
            {'from':user1, 'to':user2, 'quantity':'100000.000000 GOLOS', 'memo':''},
            providebw=user1+'/finteh@providebw',
            keys=[golosIoKey, private1])

        user1Vesting = json.loads(testnet.cleos('get table finteh.vest %s accounts' % user1))['rows'][0]
        user2Vesting = json.loads(testnet.cleos('get table finteh.vest %s accounts' % user2))['rows'][0]
        self.assertEqual(user1Vesting['delegated'], '100111.000000 GOLOS')
        self.assertEqual(user2Vesting['received'],  '111.000000 GOLOS')

        wait(120+3)  # get from params `delegation.return_time`

        user1Vesting = json.loads(testnet.cleos('get table finteh.vest %s accounts' % user1))['rows'][0]
        user2Vesting = json.loads(testnet.cleos('get table finteh.vest %s accounts' % user2))['rows'][0]
        self.assertEqual(user1Vesting['delegated'], '111.000000 GOLOS')
        self.assertEqual(user2Vesting['received'],  '111.000000 GOLOS')

        testnet.pushAction('finteh.vest', 'undelegate', user1,
            {'from':user1, 'to':user2, 'quantity':'111.000000 GOLOS', 'memo':''},
            providebw=user1+'/finteh@providebw',
            keys=[golosIoKey, private1])

        user1Vesting = json.loads(testnet.cleos('get table finteh.vest %s accounts' % user1))['rows'][0]
        user2Vesting = json.loads(testnet.cleos('get table finteh.vest %s accounts' % user2))['rows'][0]
        self.assertEqual(user1Vesting['delegated'], '111.000000 GOLOS')
        self.assertEqual(user2Vesting['received'],  '0.000000 GOLOS')

        wait(120+3)  # get from params `delegation.return_time`

        user1Vesting = json.loads(testnet.cleos('get table finteh.vest %s accounts' % user1))['rows'][0]
        user2Vesting = json.loads(testnet.cleos('get table finteh.vest %s accounts' % user2))['rows'][0]

        self.assertEqual(user1Vesting['delegated'], '0.000000 GOLOS')
        self.assertEqual(user2Vesting['received'],  '0.000000 GOLOS')



class TestGolosIoTesting(unittest.TestCase):
    def test_provideGolosToken(self):
        (priate,public) = createKey()
        user = createRandomAccount(public)

        # We need use issue to self and then transfer due error:
        #   Error 3100006: Subjective exception thrown during block production
        #   Error Details:
        #   Authorization failure with inline action sent to self
        #   transaction declares authority '{"actor":"finteh","permission":"active"}', but does not have signatures for it under a provided delay of 0 ms, provided permissions [{"actor":"cyber.token","permission":"cyber.code"}], provided keys [], and a delay max limit of 3888000000 ms
        amount = '100.000 GOLOS'
        testnet.pushAction('cyber.token', 'issue', 'finteh@issue', ['finteh', amount, ''],
                keys=[testingKey])

        testnet.pushAction('cyber.token', 'transfer', 'finteh@issue', ['finteh', user, amount, ''],
                keys=[testingKey])

        self.assertEqual(amount+'\n', testnet.cleos('get currency balance cyber.token %s GOLOS' % user))

    def test_setVestingParams(self):
        params = json.loads(testnet.cleos('get table finteh.vest GOLOS vestparams'))['rows'][0]
        min_amount = params['min_amount']['min_amount']
        min_amount += -1 if min_amount%2 else +1
        testnet.pushAction(
                'finteh.vest', 'setparams', 'finteh', ["6,GOLOS",[["vesting_amount",{"min_amount":min_amount}]]],
                keys=[testingKey])

    def test_setPostingParams(self):
        params = json.loads(testnet.cleos('get table finteh.pub finteh.pub pstngparams'))['rows'][0]
        max_comment_depth = params['max_comment_depth']['value']
        max_comment_depth += -1 if max_comment_depth%2 else +1
        testnet.pushAction(
                'finteh.pub', 'setparams', 'finteh.pub', [[["st_max_comment_depth",{"value":max_comment_depth}]]],
                providebw='finteh.pub/finteh@providebw',
                keys=[testingKey, golosIoKey])


def urlInDocker(url=testnet.params['nodeos_url']):
    r = re.match(r'http://([^:]+)(:[0-9]+)?', url)
    host = socket.gethostbyname(r.group(1))
    internalUrl = 'http://{host}{port}'.format(host=host, port=r.group(2))
    return internalUrl


class TestGolos_v2_0_1(unittest.TestCase):
    def setUp(self):
        self.image = 'cyberway/golos.contracts:stable'

    def updateGlsPublish(self):
        cmd = 'docker run -ti {image} bash -c "PATH=$PATH:/opt/cyberway/bin/; ' \
              'cleos wallet create --to-console && cleos wallet import --private-key {KEY} && ' \
              'cleos --url {URL} set contract finteh.pub /opt/golos.contracts/golos.publication --bandwidth-provider finteh.pub/finteh"'.format(
              URL=urlInDocker(), KEY=testingKey, image=self.image)
        subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT, universal_newlines=True)

    def updateGlsSocial(self):
        cmd = 'docker run -ti {image} bash -c "PATH=$PATH:/opt/cyberway/bin/; ' \
              'cleos wallet create --to-console && cleos wallet import --private-key {KEY} && ' \
              'cleos --url {URL} set contract finteh.soc /opt/golos.contracts/golos.social --bandwidth-provider finteh.soc/finteh"'.format(
              URL=urlInDocker(), KEY=testingKey, image=self.image)
        subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT, universal_newlines=True)

    def createGolosIoAuthorityTrx(self):
        trx = testnet.Trx(expiration=5*60)
    
        golosIoAuth = testnet.createAuthority([], ['finteh@golos.io'])
        codeActions = [
            ('finteh.pub', ['addpermlink', 'delpermlink', 'addpermlinks', 'delpermlinks',]),
            ('finteh.soc',  ['addpin', 'addblock',])
        ]
    
        for (code, actions) in codeActions:
            trx.addAction('cyber', 'updateauth', code, {
                    'account': code,
                    'permission': 'golos.io',
                    'parent': 'active',
                    'auth': golosIoAuth
                })
    
            trx.addAction('cyber', 'providebw', 'finteh', {
                    'provider': 'finteh',
                    'account': code
                })
    
            for act in actions:
                trx.addAction('cyber', 'linkauth', code, {
                        "account": code,
                        "code": code,
                        "type": act,
                        "requirement": "golos.io"
                    })
        print(json.dumps(trx.getTrx(),indent=3))
        return trx

    def updateGolosIoAuthority(self):
        trx = self.createGolosIoAuthorityTrx()

        proposer = 'finteh'
        name = randomName()
        auth = testnet.parseAuthority('finteh@active')
        testnet.cleos('multisig propose_trx {name} {auth} {trx} {proposer}'.format(
                name=name, auth=testnet.jsonArg([auth]), trx=testnet.jsonArg(trx.getTrx()), proposer=proposer),
                keys=[testingKey])
        testnet.cleos('multisig approve {proposer} {name} {permission}'.format(
                proposer=proposer, name=name, permission=testnet.jsonArg(auth)),
                keys=[testingKey])
        testnet.cleos('multisig exec {proposer} {name} {executer}'.format(
                proposer=proposer, name=name, executer=proposer),
                keys=[testingKey])

        wait(5)
        glsPublish = testnet.getAccount('finteh.pub')
        print(glsPublish)


    def test_addpinblock(self):
        (private1, public1) = createKey()
        user1 = createRandomAccount(public1)

        (private2, public2) = createKey()
        user2 = createRandomAccount(public2)

        testnet.pushAction('finteh.soc', 'addpin', 'finteh.soc@golos.io', {
                "pinner": user1,
                "pinning": user2
            }, providebw='finteh.soc/finteh@providebw', keys=[golosIoKey])

        testnet.pushAction('finteh.soc', 'addblock', 'finteh.soc@golos.io', {
                "blocker": user2,
                "blocking": user1
            }, providebw='finteh.soc/finteh@providebw', keys=[golosIoKey])

    def test_adddelpermlink(self):
        author = resolve("null@golos")
        permlink = randomPermlink()
        testnet.pushAction('finteh.pub', 'addpermlink', 'finteh.pub@golos.io', {
                'msg': {'author': author, 'permlink': permlink},
                'parent': {'author': '', 'permlink': ''},
                'level': 0,
                'childcount': 0
            }, providebw='finteh.pub/finteh@providebw', keys=[golosIoKey])

        testnet.pushAction('finteh.pub', 'delpermlink', 'finteh.pub@golos.io', {
                'msg': {'author': author, 'permlink': permlink}
            }, providebw='finteh.pub/finteh@providebw', keys=[golosIoKey])

    def printRewardPools(self):
        pools = json.loads(testnet.cleos('get table finteh.pub finteh.pub rewardpools -l 10'))['rows']
        for pool in pools:
            print('{created} with {fund} funds {msgs} messages and {rshares}'.format(
                    created=pool['created'], fund=pool['state']['funds'], msgs=pool['state']['msgs'], rshares=pool['state']['rshares']))

        msgs = json.loads(testnet.cleos('get table finteh.pub finteh.pub message -l 50'))['rows']
        for msg in msgs:
            print('{pool_date} {date} {author}/{ident}'.format(
                    pool_date=msg['pool_date'], date=msg['date'], author=msg['author'], ident=msg['id']))

    @unittest.skipUnless(os.environ.get("RUN_SKIPPED",False), "Test skipped due it should be executed on empty testnet")
    def test_syncpool(self):
        # Description: test for syncpool mechanics:
        # Precondition: golos testnet with no closed posts (cyberway/golos.contracts:ci-skip-posts)
        # Check:
        #   - fix message count broken by deletemssg action
        #   - fix difference between finteh.pub GOLOS balance and reward-pools state

        print('--- Change reward window settings & emission period ---')
        params = json.loads(testnet.cleos('get table finteh.pub finteh.pub pstngparams'))['rows'][0]
        if params['cashout_window']['window'] != 60 or params['cashout_window']['upvote_lockout'] != 5:
            testnet.pushAction('finteh.pub', 'setparams', 'finteh.pub', {
                    'params':[
                        ['st_cashout_window', {'window': 60, 'upvote_lockout': 5}]
                    ]
               }, providebw='finteh.pub/finteh', keys=[testingKey])

        params = json.loads(testnet.cleos('get table finteh.emit finteh.emit emitparams'))['rows'][0]
        if params['interval']['value'] != 15:
            testnet.pushAction('finteh.emit', 'setparams', 'finteh.emit', {
                    'params':[
                        ['emit_interval', {'value': 15}]
                    ]
                }, providebw='finteh.emit/finteh', keys=[testingKey])

        self.printRewardPools()

        print('--- Set rules for new pool ---')
        rules = {
            "mainfunc": {
                "str": "x",
                "maxarg": "2251799813685247"
            },
            "curationfunc": {
                "str": "x",
                "maxarg": "2251799813685247"
            },
            "timepenalty": {
                "str": "1",
                "maxarg": 1
            },
            "maxtokenprop": 5000,
            "tokensymbol": "3,GOLOS"
        }
        testnet.pushAction('finteh.pub', 'setrules', 'finteh.pub', rules, providebw='finteh.pub/finteh', keys=[testingKey])

        self.printRewardPools()

        print('--- Create some accounts ---')
        accounts = {}
        for i in range(5):
            (private, public) = createKey()
            acc = createRandomAccount(public)
            accounts[acc] = private
            testnet.openVestingBalance(acc, 'finteh', keys=[testingKey])
            testnet.issueToken(acc, '100.000 GOLOS', providebw=acc+'/finteh', keys=[testingKey])
            testnet.transfer(acc, 'finteh.vest', '100.000 GOLOS', '', providebw=acc+'/finteh', keys=[private, testingKey])

        print('--- Create posts ---')
        pauthor = random.choice(list(accounts))
        ppermlink = randomPermlink()
        testnet.createPost(pauthor, ppermlink, 'ru', randomText(12), randomText(32),
                providebw=pauthor+'/finteh@providebw', keys=[golosIoKey, accounts[pauthor]])

        posts = []
        for i in range(5):
            author = random.choice(list(accounts))
            permlink = randomPermlink()
            testnet.createComment(author, permlink, pauthor, ppermlink, randomText(12), randomText(32),
                    providebw=author+'/finteh@providebw', keys=[golosIoKey, accounts[author]])
            # testnet.upvotePost(author, author, permlink, 10000, providebw=author+'/finteh@providebw', keys=[golosIoKey, accounts[author]])
            posts.append((author, permlink,))
            wait(10)

        self.printRewardPools()

        print('--- Remove some posts ---')
        print('posts has {count}'.format(count=len(posts)))
        for i in range(2):
            (author, permlink) = random.choice(posts)
            posts.remove((author, permlink,))
            testnet.pushAction('finteh.pub', 'deletemssg', author, {
                    'message_id': {'author': author, 'permlink': permlink}
                }, providebw=author+'/finteh@providebw', keys=[golosIoKey, accounts[author]])
        print('posts has {count}'.format(count=len(posts)))

        self.printRewardPools()

        print('--- Wait for close some posts ---')
        wait(60)

        self.printRewardPools()

        print('--- Set new rules (broken) ---')
        rules = {
            "mainfunc": {
                "str": "((x + 4000000000000) / (x + 8000000000000)) * (x / 4096)",
                "maxarg": "2000000000000000"
            },
            "curationfunc": {
                "str": "x",
                "maxarg": "2251799813685247"
            },
            "timepenalty": {
                "str": "1",
                "maxarg": 1
            },
            "maxtokenprop": 5000,
            "tokensymbol": "6,GOLOS"
        }
        testnet.pushAction('finteh.pub', 'setrules', 'finteh.pub', rules, providebw='finteh.pub/finteh', keys=[testingKey])
        
        self.printRewardPools()

        print('--- Wait some time (periodically create posts) ---')
        for i in range(10):
            permlink = randomPermlink()
            testnet.createComment(author, permlink, pauthor, ppermlink, randomText(12), randomText(32),
                    providebw=author+'/finteh@providebw', keys=[golosIoKey, accounts[author]])
            # testnet.upvotePost(author, author, permlink, 10000, providebw=author+'/finteh@providebw', keys=[golosIoKey, accounts[author]])
            posts.append((author, permlink,))
            wait(10)

        self.printRewardPools()

        print('--- Remove some posts ---')
        for i in range(2):
            (author, permlink) = random.choice(posts)
            posts.remove((author, permlink,))
            testnet.pushAction('finteh.pub', 'deletemssg', author, {
                    'message_id': {'author': author, 'permlink': permlink}
                }, providebw=author+'/finteh@providebw', keys=[golosIoKey, accounts[author]])

        
        # Check pool state

        self.printRewardPools()

        print('--- Update finteh.pub contract ---')
        self.updateGlsPublish()

        self.printRewardPools()

        print('--- Set new rules (fixed) ---')
        rules['tokensymbol'] = '3,GOLOS'
        testnet.pushAction('finteh.pub', 'setrules', 'finteh.pub', rules, providebw='finteh.pub/finteh', keys=[testingKey])

        self.printRewardPools()

        print('--- Execute `syncpool` ---')
        testnet.pushAction('finteh.pub', 'syncpool', 'finteh.pub', {}, providebw='finteh.pub/finteh', keys=[testingKey])

        self.printRewardPools()

        print('--- Remove some posts ---')
        for i in range(2):
            (author, permlink) = random.choice(posts)
            posts.remove((author, permlink,))
            testnet.pushAction('finteh.pub', 'deletemssg', author, {
                    'message_id': {'author': author, 'permlink': permlink}
                }, providebw=author+'/finteh@providebw', keys=[golosIoKey, accounts[author]])

        self.printRewardPools()

        print('--- Wait some time ---')
        for i in range(15):
            permlink = randomPermlink()
            testnet.createComment(author, permlink, pauthor, ppermlink, randomText(12), randomText(32),
                    providebw=author+'/finteh@providebw', keys=[golosIoKey, accounts[author]])
            # testnet.upvotePost(author, author, permlink, 10000, providebw=author+'/finteh@providebw', keys=[golosIoKey, accounts[author]])
            posts.append((author, permlink,))
            wait(10)

        # Check pool state


if __name__ == '__main__':
    unittest.main()
