## Deployment

1. Clone the repository with submodule initialization:
```bash
git clone --recurse-submodules https://github.com/fincubator/golos.contracts
cd golos.contracts
```
2. Start build script:
```bash
env BUILDKITE_BRANCH=master CI=false DOCKER_BUILDKIT=1 .buildkite/steps/build-image.sh
```
3. Run bash in container:
```bash
docker run -it cyberway/golos.contracts:$(git rev-parse HEAD) bash
```
4. Go to cleos directory:
```bash
cd /opt/cyberway/bin
```
5. Create cleos wallet:
```bash
./cleos wallet create --to-console
```
Save password from the output to use in the future to unlock this wallet.  
6. Import private key of your cyberway account:
```bash
./cleos wallet import
```
7. Generate key pair of a new prefix account for contracts:
```bash
./cleos create key --to-console
```
Repeat this once more if your want to use separate owner and active keys.  
8. Bid to buy name for prefix:
```bash
./cleos -u <node> system bidname <bidder> <prefix> "1.0000 CYBER"
```
In place of `<node>` choose one of the working nodeos URLs from https://github.com/fincubator/cyberway.launch/blob/master/apinodes.  
For example, the following command will bid name `finteh` from `fintehescrow` using node http://46.148.232.188:8888:
```bash
./cleos -u http://46.148.232.188:8888 system bidname fintehescrow finteh "1.0000 CYBER"
```
9. Create prefix account:
```bash
./cleos -u <node> create account <bidder> <prefix> <OwnerKey> [ActiveKey]
```
Use private keys generated earlier.  
10. Create accounts for all contracts and import their keys into wallet:
  * golos.charge
  * golos.config
  * golos.ctrl
  * golos.emit
  * golos.memo
  * golos.publication
  * golos.referral
  * golos.social
  * golos.vesting  
Each account name should not have more than 12 characters (including prefix).
11. Set contract:
```bash
./cleos -u <node> set contract <account> /opt/golos.contracts/<contract> --abi <contract>.abi
```
Repeat this for all contracts.  
For example, the following command will set contract `golos.charge` on account `finteh.chrg` using node http://46.148.232.188:8888:
```bash
./cleos -u http://46.148.232.188:8888 set contract finteh.chrg /opt/golos.contracts/golos.charge --abi golos.charge.abi
```
