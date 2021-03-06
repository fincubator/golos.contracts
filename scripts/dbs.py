import os
from pymongo import MongoClient
import pymongo
from config import *
from decimal import Decimal
from bson.decimal128 import Decimal128

golos_url = os.environ.get("GOLOS_DB", "mongodb://172.17.0.1:27017")
cyberway_url = os.environ.get("CYBERWAY_DB", "mongodb://127.0.0.1:27018")

golos_client = MongoClient(golos_url)
cyberway_client = MongoClient(cyberway_url)

golos_db = golos_client['Golos']
cyberway_db = cyberway_client[CYBERWAY_PREFIX]
cyberway_cyber_token_db = cyberway_client[CYBERWAY_PREFIX+'cyber_token']
cyberway_gls_vesting_db = cyberway_client[CYBERWAY_PREFIX+'gls_vesting']
cyberway_gls_ctrl_db = cyberway_client[CYBERWAY_PREFIX+'gls_ctrl']
cyberway_gls_publish_db = cyberway_client[CYBERWAY_PREFIX+'gls_publish']

def createIndexes(collection, indexes):
    db_indexes = collection.index_information()
    for (key,unique) in indexes:
        name = 'convert_%s'%key
        if name not in db_indexes:
            collection.create_index(key, name=name, unique=unique)

def get_next_id(collection):
    item = collection.find_one(sort=[('id',pymongo.DESCENDING)])
    return int(item['id'].to_decimal())+1 if item else 0

class Table:
    def __init__(self, table, getNextId, indexes):
        self.table = table
        self.cache = []
        if getNextId:
            self.next_id = getNextId(table)
        if indexes:
            createIndexes(table, indexes)

    def nextId(self):
        current = self.next_id
        self.next_id = self.next_id + 1
        return current

    def append(self, item):
        self.cache.append(item)

    def writeCache(self):
        if len(self.cache):
            self.table.insert_many(self.cache)
            self.cache = []

class Tables:
    def __init__(self,init_list):
        for (alias, table, getNextId, indexes) in init_list:
            setattr(self, alias, Table(table,getNextId,indexes))

    def writeCache(self):
        for name, table in self.__dict__.items():
            table.writeCache()
