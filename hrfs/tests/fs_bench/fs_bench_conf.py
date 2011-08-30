import MySQLdb;

db=MySQLdb.connect(host="40.0.2.100",user="root",passwd="123456",db="fs_bench");

debug = 1;

#
# _ONLY_ after you have configured the proper partition (in fs_bench.py), change this value to 0. Otherwise
# you may lose data! Don't say I didn't warn you. 
#
dry_run = 0;
