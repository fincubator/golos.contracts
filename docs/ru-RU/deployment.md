## Развёртывание

1. Клонируйте репозиторий с инициализацией подмодулей:
```bash
git clone --recurse-submodules https://github.com/fincubator/golos.contracts
cd golos.contracts
```
2. Начните скрипт сборки:
```bash
env BUILDKITE_BRANCH=master CI=false DOCKER_BUILDKIT=1 .buildkite/steps/build-image.sh
```
3. Запустите bash в контейнере:
```bash
docker run -it cyberway/golos.contracts:$(git rev-parse HEAD) bash
```
4. Перейдите в директорию cleos:
```bash
cd /opt/cyberway/bin
```
5. Создайте кошелёк cleos:
```bash
./cleos wallet create --to-console
```
Сохраните пароль из вывода чтобы использовать его в будущем для разблокирования кошелька.
6. Импортируйте приватный ключ вашего аккаунта cyberway:
```bash
./cleos wallet import
```
7. Сгенерируйте пару ключей нового аккаунта-префикса для контрактов:
```bash
./cleos create key --to-console
```
Повторите ещё раз, если хотите, чтобы ключ владельца и активный ключ были различными.
8. Поставьте на покупку имени для префикса:
```bash
./cleos -u <node> system bidname <bidder> <prefix> "1.0000 CYBER"
```
Вместо `<node>` выберите один из рабочих адресов nodeos из https://github.com/fincubator/cyberway.launch/blob/master/apinodes.  
Например, следующая команда поставит на имя `finteh` от `fintehescrow`, используя ноду http://46.148.232.188:8888:
```bash
./cleos -u http://46.148.232.188:8888 system bidname fintehescrow finteh "1.0000 CYBER"
```
9. Создайте аккаунт-префикс:
```bash
./cleos -u <node> create account <bidder> <prefix> <OwnerKey> [ActiveKey]
```
Используйте приватные ключи, сгенерированные ранее.
10. Создайте аккаунты для всех контрактов с префиксом и импортируйте их ключи в кошелёк:
  * golos.charge
  * golos.config
  * golos.ctrl
  * golos.emit
  * golos.memo
  * golos.publication
  * golos.referral
  * golos.social
  * golos.vesting
Каждое имя аккаунта не должно иметь более 12 символов (включая префикс).
11. Установите контракт:
```bash
./cleos -u <node> set contract <account> /opt/golos.contracts/<contract> --abi <contract>.abi
```
Повторите для всех контрактов.  
Например, следующая команда установит контракт `golos.charge` на аккаунте `finteh.chrg`, используя ноду http://46.148.232.188:8888:
```bash
./cleos -u http://46.148.232.188:8888 set contract finteh.chrg /opt/golos.contracts/golos.charge --abi golos.charge.abi
```
