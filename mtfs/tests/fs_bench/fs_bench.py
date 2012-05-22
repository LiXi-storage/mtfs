#!/usr/bin/python
#
# Author: Mike Benoit, mikeb@netnation.com
#
# Version: 0.07 - 24-Oct-2003
#
# THIS SCRIPT COMES WITH ABSOLUTELY NO WARRANTY, USE AT YOUR OWN RISK
#
# License: GPL
#
# Copyright 2003 - Netnation Communications/Hostway
#
"""
*****************************************************************************************

WARNING, THIS SCRIPT WILL *ERASE* WHATEVER PARTITION YOU SPECIFY! MAKE SURE YOU GET THE RIGHT ONE! ;)

*****************************************************************************************


The reason I created this little script was because I was tired of searching high and low
for non-complete file system benchmarks. It seemed I could never find benchmarks that were
ever close to the "real-world" situation I was looking for. The benchmarks I could find
always seemed to be biased in some form, or ommiting "certain" information.

Like reiserfs v3 against ext3 with really small files. But never medium/large files. And
never all the main file systems pitted against one another on the same page.

One file system is never faster in absolutely every situation, so a multitude of tests
need to be run with many different options to get the full picture. Therefore the
purpose of this script is to make it really easy, and completely automated to benchmark
any number of file systems, or even file system options against one another with any
number of different benchmarks and benchmark settings, in hopes of seeing much more
complete benchmarks.

Yes, running this many tests takes days and days, but thats why this script
should automate it all for you, and at least you get enough data to see where the
strengths/weaknesses are of each different file system. Thats the idea anyways.
"""

"""
Database Schema
---------------------------------------------------
CREATE TABLE fs_bench (
    id int(11) NOT NULL auto_increment,
    
    bench_id int(11) default NULL,
    bench_program varchar(100) NOT NULL,

    batch_id int(11) default NULL,    
    
    host_name varchar(100) default NULL,
    kernel_version varchar(100) default NULL,
    
    fs_type varchar(100) default NULL,
    fs_version varchar(100) default NULL,
    
    PRIMARY KEY  (id)
) TYPE=MyISAM;

CREATE TABLE bonnie (
  id int(11) NOT NULL auto_increment,
  run_id int(11) default NULL,
  cmd varchar(250) default NULL,
  start_time double(15,4) default NULL,
  end_time double(15,4) default NULL,
  file_size varchar(10) default NULL,
  putc int(10) default NULL,
  putc_cpu int(10) default NULL,
  put_block int(10) default NULL,
  put_block_cpu int(10) default NULL,
  rewrite int(10) default NULL,
  rewrite_cpu int(10) default NULL,
  getc int(10) default NULL,
  getc_cpu int(10) default NULL,
  get_block int(10) default NULL,
  get_block_cpu int(10) default NULL,
  seeks int(10) default NULL,
  seeks_cpu int(10) default NULL,
  num_files varchar(250) default NULL,
  seq_create int(10) default NULL,
  seq_create_cpu int(10) default NULL,
  seq_stat int(10) default NULL,
  seq_stat_cpu int(10) default NULL,
  seq_del int(10) default NULL,
  seq_del_cpu int(10) default NULL,
  ran_create int(10) default NULL,
  ran_create_cpu int(10) default NULL,
  ran_stat int(10) default NULL,
  ran_stat_cpu int(10) default NULL,
  ran_del int(10) default NULL,
  ran_del_cpu int(10) default NULL,
  PRIMARY KEY  (id)
) TYPE=MyISAM;

CREATE TABLE iozone (
  id int(11) NOT NULL auto_increment,
  run_id int(11) default NULL,
  cmd varchar(250) default NULL,
  start_time double(15,4) default NULL,
  end_time double(15,4) default NULL,
  file_size varchar(10) default NULL,
  reclen varchar(250) default NULL,
  io_write int(10) default NULL,
  io_rewrite int(10) default NULL,
  io_read int(10) default NULL,
  io_reread int(10) default NULL,
  ran_read int(10) default NULL,
  ran_write int(10) default NULL,
  bkwd_read int(10) default NULL,
  record_rewrite int(10) default NULL,
  stride_read int(10) default NULL,
  fwrite int(10) default NULL,
  frewrite int(10) default NULL,
  fread int(10) default NULL,
  freread int(10) default NULL,
  PRIMARY KEY  (id)
) TYPE=MyISAM;
"""


from fs_bench_conf import *;
import time, sys, os, popen2, fcntl, re, string;
import pprint;

def print_debug(str):
    global debug;

    if debug == 1:
        print str;

def getCommandOutput(command):
        global dry_run;
        print_debug("getCommandOutput(): Command: "+ command);

        child = os.popen(command)
        data = child.read()
        err = child.close()
        
        if err:
            raise RuntimeError, '%s failed w/ exit code %d' % (command, err)
        
        return data

class fs_bench:
    #
    #
    # CHANGE THESE!!!!
    #
    #
    
    #This partition will be completely ERASED!!!
    #This partition will be completely ERASED!!!
    partition = "loop1";
    mount_point = "/home/test2";
    user = "mikeb";
    group = "mikeb";
    
    #File systems to test
    #file_systems = ["ext2", "jfs", "xfs", "reiser4", "reiser4_extents", "reiserfs", "reiserfs_notail", "ext3_journal", "ext3_ordered", "ext3_writeback"];
    file_systems = ["ext2", "ext3_journal", "ext3_ordered", "ext3_writeback"];

    #Benchmark programs to run with each different file system
    #bench_programs = ["bonnie", "iozone"];
    bench_programs = ["bonnie"];
    
    #How many times to run each benchmark program. Results are averaged in the reports.
    #I've found that the longer tests really don't vary that much, so 1 run is pretty close.
    runs_per_bench_program = 1;
    
    
    fs_options = {}; #Stateful options for each file system bench.
    removed_bench_programs = [];
    batch = 0;
    run = 0;
    
    def dry_run(self):
        global dry_run;
    
        return dry_run;

    def __init__(self):
        global db;
        
        print_debug("Class: %s::" % (self.__class__,));

        cursor=db.cursor();
        query="select max(bench_id) from fs_bench";
        cursor.execute(query);
        try:
            bench_id = cursor.fetchone()[0] + 1;
        except:
            bench_id = 0;
        print_debug("Class: %s:: Next Bench ID: %s" % (self.__class__,bench_id));

        self.hostname = self.get_hostname();
        self.kernel_version = self.get_kernel_version();
        
        self.partition_size = self.get_partition_size();
        self.memory_size = self.get_memory_size();

        while len(self.removed_bench_programs) < len(self.bench_programs):
            for bench_program in self.bench_programs:
                if bench_program not in self.removed_bench_programs:
                    print_debug("Class: %s::Current Bench Program: %s" % (self.__class__,bench_program));
                    
                    bench = eval(bench_program+"()");
                    
                    #Loop over file systems, running each test....
                    for file_system in self.file_systems:
                        print_debug("\nClass: %s::Current File System: %s Batch: %s" % (self.__class__,file_system, self.batch));
                        
                        fs = eval(file_system+"()");
                        print_debug("Class: %s::Current File System Version: %s" % (self.__class__,fs.version));            

                        query="""insert into fs_bench ( bench_id,
                                                        bench_program,
                                                        batch_id,
                                                        host_name,
                                                        kernel_version,
                                                        fs_type,
                                                        fs_version)
                                                VALUES (%s,
                                                        '%s',
                                                        %s,
                                                        '%s',
                                                        '%s',
                                                        '%s',
                                                        '%s')""" % (bench_id,bench_program,self.batch,self.hostname,self.kernel_version,file_system,fs.version);
                        print_debug("Class: %s:: Query: %s" % (self.__class__,query));
                        cursor.execute(query);
                        #LIXI: cursor has no inster_id
                        #id = cursor.insert_id();
                        id = db.insert_id();
                        print_debug("Class: %s:: Insert ID: %s" % (self.__class__,id));
                        
                        #Re-Create FS between runs... I believe this helps clear the cache.
                        for i in range(self.runs_per_bench_program):
                            print_debug("Class: %s:: Run Per Bench: %s" % (self.__class__,i));
                            print_debug("Class: %s::run(): Time: %s" % (self.__class__,time.ctime()));
                            
                            fs.unmount();
                            fs.mkfs();
                            fs.mount();
                            
                            bench.run(self.batch, id);
                            
                            fs.unmount();
                        
                        print_debug("Class: %s:: Sleeping..." % (self.__class__,));
                        time.sleep(1);
                        
                        self.run += 1;
                        
                    self.batch += 1;
    
    def remove_bench_program(self,name):
        self.removed_bench_programs.append(name);
        print_debug("Class: %s:: remove_bench_program(): Removing: %s --------------------------------------" % (self.__class__,name));
        #pprint.pprint(self.removed_bench_programs);
        time.sleep(2);
        
    def get_partition_size(self):
        print_debug("Class: %s::%s()" % (self.__class__,"get_partition_size"));
        partition_size = int(getCommandOutput("fdisk -s /dev/"+ self.partition));
        print_debug("Class: %s::%s(): Partition Size: %s" % (self.__class__,"get_partition_size", partition_size));
        
        return partition_size; 
        
    def get_memory_size(self):
        print_debug("Class: %s::%s()" % (self.__class__,"get_memory_size"));
        memory_size = int(getCommandOutput("cat /proc/meminfo | head -n1 | tr -s ' ' | cut -d' ' -f2"));
        print_debug("Class: %s::%s(): Memory Size: %s" % (self.__class__,"get_memory_size", memory_size));
        
        return memory_size;
    
    def get_kernel_version(self):
        print_debug("Class: %s::%s()" % (self.__class__,"get_kernel_version"));
        kernel_version = string.strip( getCommandOutput("uname -r") );
        print_debug("Class: %s::%s(): Kernel Version: %s" % (self.__class__,"get_kernel_version", kernel_version));
        
        return kernel_version;

    def get_hostname(self):
        print_debug("Class: %s::%s()" % (self.__class__,"get_hostname"));
        hostname = string.strip( getCommandOutput("hostname") );
        print_debug("Class: %s::%s(): Hostname: %s" % (self.__class__,"get_hostname", hostname));
        
        return hostname;

#
#
# Benchmark Programs...
#
#
class bonnie(fs_bench):
    name = "bonnie";
    
    size_list = [1024, 2048, 4096];
    files_list = [10, 25, 50, 100];

    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.max_size = self.get_partition_size() / 1024;
        try:
            self.fs_options[self.name]
        except:
            self.fs_options[self.name] = {"batch": 0, "size": self.size_list[0], "files": self.files_list[0]};
        
    def options(self, batch):
        print_debug("Class: %s::options(): Batch: %s" % (self.__class__,batch));

        size = self.fs_options[self.name]["size"];
        files = self.fs_options[self.name]["files"];
       
        if batch != self.fs_options[self.name]["batch"]:
            print_debug("Class: %s::options(): Batch Changed.....: %s" % (self.__class__,batch));

            for tmp_files in self.files_list:
                
                if ( tmp_files > self.fs_options[self.name]["files"]):
                    files = tmp_files;
                    break;
                elif (files >= self.files_list[len(self.files_list)-1]):
                    files = self.files_list[0];
                
                    for tmp_size in self.size_list:
                        
                        if ( tmp_size > self.fs_options[self.name]["size"]):
                            size = tmp_size;
                            break;
                        elif (tmp_size >= self.size_list[len(self.size_list)-1]):
                            self.remove_bench_program(self.name);

            print_debug("Class: %s::options(): Size: %s Files: %s" % (self.__class__, size, files));
            
            self.fs_options[self.name] = {"size": size, "files": files};
                   
        self.fs_options[self.name]["batch"] = batch;
        
    def parse(self,output,id, cmd, start_time, end_time):
        global db;
        
        print_debug("Class: %s::parse(): ID: %s " % (self.__class__, id));

        values = string.split(output, ",");

        i = 0;
        for value in values:
            if re.match("\+{1,10}", value):
                values[i] = -1;
            elif value == '':
                values[i] = -1;
            i += 1;
                
        query = """insert into bonnie (
                                        run_id,
                                        cmd,
                                        start_time,
                                        end_time,
                                        file_size,
                                        putc,
                                        putc_cpu,
                                        put_block,
                                        put_block_cpu,
                                        rewrite,
                                        rewrite_cpu,
                                        getc,
                                        getc_cpu,
                                        get_block,
                                        get_block_cpu,
                                        seeks,
                                        seeks_cpu,
                                        num_files,
                                        seq_create,
                                        seq_create_cpu,
                                        seq_stat,
                                        seq_stat_cpu,
                                        seq_del,
                                        seq_del_cpu,
                                        ran_create,
                                        ran_create_cpu,
                                        ran_stat,
                                        ran_stat_cpu,
                                        ran_del,
                                        ran_del_cpu)
                                VALUES (%s,
                                        '%s',
                                        %s,
                                        %s,
                                        '%s',
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        '%s',
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s)""" % (id, cmd, start_time, end_time, values[1], values[2],values[3],values[4],values[5],values[6],values[7],values[8],values[9],values[10],values[11],values[12],values[13],values[14],values[15],values[16],values[17],values[18],values[19],values[20],values[21],values[22],values[23],values[24],values[25],values[26]);
        #print_debug("Class: %s::parse(): Query: %s " % (self.__class__, query));
        cursor=db.cursor();
        cursor.execute(query);
        
    def run(self, batch, id):
        print_debug("Class: %s::run(): Batch: %s" % (self.__class__,batch));
        
        self.options(batch);
        
        #cmd = "/sbin/bonnie++ -f -u ipso -d /home/test2 -r 10 -s 20 -n 1 -x 1";
        cmd = "/sbin/bonnie++ -u %s -d %s -s %s -n %s:100000:10:%s -x 1" % (self.user, self.mount_point, self.fs_options[self.name]["size"], self.fs_options[self.name]["files"], self.fs_options[self.name]["files"]);

        if self.dry_run() == 0:
            
            try:
                start_time = time.time();
                output = getCommandOutput(cmd);
                end_time = time.time();
                
                self.parse(output,id, cmd, start_time, end_time);
            except RuntimeError:
                1;
            print_debug("Class: %s::run(): Output: %s" % (self.__class__,output));

class iozone(fs_bench):
    name = "iozone";
    
    size_list = [1024, 2048, 4096];
    files_list = [64, 512, 1024, 4096, 16348];
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.max_size = self.get_partition_size() / 1024;
        try:
            self.fs_options[self.name]
        except:
            self.fs_options[self.name] = {"batch": 0, "size": self.size_list[0], "files": self.files_list[0]};
        
    def options(self, batch):
        print_debug("Class: %s::options(): Batch: %s" % (self.__class__,batch));

        size = self.fs_options[self.name]["size"];
        files = self.fs_options[self.name]["files"];

        #print_debug("Size: %s") % (size);
        if batch != self.fs_options[self.name]["batch"]:
            print_debug("Class: %s::options(): Batch Changed.....: %s" % (self.__class__,batch));

            for tmp_files in self.files_list:
                
                if ( tmp_files > self.fs_options[self.name]["files"]):
                    files = tmp_files;
                    break;
                elif (files >= self.files_list[len(self.files_list)-1]):
                    files = self.files_list[0];
                
                    for tmp_size in self.size_list:
                        
                        if ( tmp_size > self.fs_options[self.name]["size"]):
                            size = tmp_size;
                            break;
                        elif (tmp_size >= self.size_list[len(self.size_list)-1]):
                            self.remove_bench_program(self.name);

            print_debug("Class: %s::options(): Size: %s Files: %s" % (self.__class__, size, files));
            
            self.fs_options[self.name] = {"size": size, "files": files};
            
            #pprint.pprint(self.fs_options);        
        
        self.fs_options[self.name]["batch"] = batch;
        
    def parse(self,output,id, cmd, start_time, end_time):
        global db;
        
        print_debug("Class: %s::parse(): ID: %s " % (self.__class__, id));

        values = string.split(output, " ");

        query = """insert into iozone (
                                        run_id,
                                        cmd,
                                        start_time,
                                        end_time,
                                        file_size,
                                        reclen,
                                        io_write,
                                        io_rewrite,
                                        io_read,
                                        io_reread,
                                        ran_read,
                                        ran_write,
                                        bkwd_read,
                                        record_rewrite,
                                        stride_read,
                                        fwrite,
                                        frewrite,
                                        fread,
                                        freread)
                                VALUES (%s,
                                        '%s',
                                        %s,
                                        %s,
                                        '%s',
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s,
                                        %s)""" % (id, cmd, start_time, end_time, values[1], values[2],values[3],values[4],values[5],values[6],values[7],values[8],values[9],values[10],values[11],values[12],values[13],values[14],values[15]);
        #print_debug("Class: %s::parse(): Query: %s " % (self.__class__, query));
        cursor=db.cursor();
        cursor.execute(query);
        
    def run(self, batch, id):
        print_debug("Class: %s::run()" % (self.__class__,));
        
        self.options(batch);
        
        pre_cmd = "cd %s;" % (self.mount_point,);
        cmd = "iozone -s %sM -r %s" % (self.fs_options[self.name]["size"], self.fs_options[self.name]["files"]);
        #cmd = "cd %s;iozone -s 512k -r 10";
        post_cmd = " | tail -n3 | head -n1 | tr -s \" \"";
        
        if self.dry_run() == 0:
            start_time = time.time();
            output = getCommandOutput(pre_cmd+cmd+post_cmd);
            end_time = time.time();
            
            self.parse(output,id, cmd, start_time, end_time);

            print_debug("Class: %s::run(): Output: %s" % (self.__class__,output));

#
#
# File Systems...
#
#
class ext2(fs_bench):
    name = "ext2";
    mount_fs_name = "ext2";
    mount_options = "";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();

    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("mkfs.ext2 /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);
        
    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkfs.ext2 -V 2>&1");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1,2}\.[0-9]{1,2})", output);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;

class ext3_ordered(fs_bench):
    name = "ext3_ordered";
    mount_fs_name = "ext3";
    mount_options = "data=ordered";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();
    
    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("mkfs.ext3 /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" -o "+ self.mount_options +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);

    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkfs.ext3 -V 2>&1");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1,2}\.[0-9]{1,2})", output);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;

class ext3_ordered_ext_journal(fs_bench):
    name = "ext3_ordered_ext_journal";
    mount_fs_name = "ext3";
    mount_options = "data=ordered";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();
    
    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("dd if=/dev/zero of=/dev/ramdisk bs=1k count=32768");
            getCommandOutput("mke2fs -O journal_dev /dev/ramdisk");
            getCommandOutput("mkfs.ext3 -J device=/dev/hdz99 /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" -o "+ self.mount_options +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);

    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkfs.ext3 -V 2>&1");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1,2}\.[0-9]{1,2})", output);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;

class ext3_journal(fs_bench):
    name = "ext3_journal";
    mount_fs_name = "ext3";
    mount_options = "data=journal";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();
    
    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("mkfs.ext3 /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" -o "+ self.mount_options +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);

    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkfs.ext3 -V 2>&1");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1,2}\.[0-9]{1,2})", output);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;

class ext3_writeback(fs_bench):
    name = "ext3_writeback";
    mount_fs_name = "ext3";
    mount_options = "data=writeback";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();
    
    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("mkfs.ext3 /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" -o "+ self.mount_options +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);

    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkfs.ext3 -V 2>&1");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1,2}\.[0-9]{1,2})", output);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;


class xfs(fs_bench):
    name = "xfs";
    mount_fs_name = "xfs";
    mount_options = "";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();
    
    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("mkfs.xfs -f /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);

    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkfs.xfs -V 2>&1");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1,2}\.[0-9]{1,2}\.[0-9]{1,2})", output);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;

class jfs(fs_bench):
    name = "jfs";
    mount_fs_name = "jfs";
    mount_options = "";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();
    
    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("mkfs.jfs -q /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);

    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkfs.jfs -V 2>&1");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1,2}\.[0-9]{1,2}\.[0-9]{1,2})", output);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;

class reiserfs(fs_bench):
    name = "reiserfs";
    mount_fs_name = "reiserfs";
    mount_options = "";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();
    
    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("mkfs.reiserfs -f /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);

    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkfs.reiserfs -V 2>&1");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1,2}\.[0-9]{1,2}\.[0-9]{1,2})", output);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;

class reiserfs_ext_journal(fs_bench):
    name = "reiserfs_ext_journal";
    mount_fs_name = "reiserfs";
    mount_options = "";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();
    
    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("dd if=/dev/zero of=/dev/ramdisk bs=1k count=32768");
            getCommandOutput("mkfs.reiserfs -j /dev/ramdisk -f /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);

    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkfs.reiserfs -V 2>&1");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1,2}\.[0-9]{1,2}\.[0-9]{1,2})", output);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;

class reiserfs_notail(fs_bench):
    name = "reiserfs_notail";
    mount_fs_name = "reiserfs";
    mount_options = "notail";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();
    
    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("mkfs.reiserfs -f /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" -o "+ self.mount_options +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);

    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkfs.reiserfs -V 2>&1");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1,2}\.[0-9]{1,2}\.[0-9]{1,2})", output);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;

class reiser4(fs_bench):
    name = "reiser4";
    mount_fs_name = "reiser4";
    mount_options = "";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();
    
    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("mkfs.reiser4 -fq /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);

    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkfs.reiser4 -V 2>&1");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1,2}\.[0-9]{1,2}\.[0-9]{1,2})", output);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;

class reiser4_extents(fs_bench):
    name = "reiser4_extents";
    mount_fs_name = "reiser4";
    mount_options = "";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();
    
    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("mkfs.reiser4 -fq -o policy=extents /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);

    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkfs.reiser4 -V 2>&1");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1,2}\.[0-9]{1,2}\.[0-9]{1,2})", output);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;

class vfat(fs_bench):
    name = "vfat";
    mount_fs_name = "vfat";
    mount_options = "";
    
    def __init__(self):
        print_debug("Class: %s::" % (self.__class__,));
        self.version = self.version();
    
    def mkfs(self):
        print_debug("Class: %s::mkfs" % (self.__class__,));
        if self.dry_run() == 0:
            getCommandOutput("mkfs.vfat -F 32 /dev/"+ fs_bench.partition);

    def mount(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("mount -t "+ self.mount_fs_name +" /dev/"+ fs_bench.partition +" "+ fs_bench.mount_point);
            self.chown();

    def chown(self):
        print_debug("Class: %s::" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("chown -R "+fs_bench.user+":"+fs_bench.group+" "+ fs_bench.mount_point);

    def unmount(self):
        print_debug("Class: %s::Unmount" % (self.__class__,));
        if self.dry_run() == 0:        
            getCommandOutput("umount /dev/"+ fs_bench.partition +" | echo");
        
    def version(self):
        print_debug("Class: %s::Version():" % (self.__class__,));
        output = getCommandOutput("mkdosfs 2>&1 > /tmp/mkfs.vfat.tmp; cat /tmp/mkfs.vfat.tmp");
        print_debug("Class: %s::Version(): Output: %s" % (self.__class__,output));
        
        match = re.match(".*([0-9]{1}\.[0-9]{1}).*", output, re.VERBOSE|re.MULTILINE|re.DOTALL);
        if match:
            version = match.groups()[0]
            print_debug("Class: %s::Version(): Found version: %s" % (self.__class__,version));
        else:
            print_debug("Class: %s::Version(): NO Found version" % (self.__class__,));
            
        return version;


#
#
# Main...
#
#
try:
    bench = fs_bench();
except KeyboardInterrupt:
    1;
    
print "Done...";
sys.exit(0);
