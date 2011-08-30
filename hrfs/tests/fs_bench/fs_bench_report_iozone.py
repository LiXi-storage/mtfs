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
from fs_bench_conf import *;
from fs_bench_global import *;
import time, sys, os, re, string;
import pprint;

query = """select max(bench_id) from fs_bench where bench_program = 'iozone'""";
cursor=db.cursor();
cursor.execute(query);

max_bench_id = cursor.fetchone()[0];
#max_bench_id = 149;

query = """	select
			a.host_name,
			a.fs_type,
			b.file_size,
			avg(b.reclen),
			avg(b.io_write),
			avg(b.io_rewrite),
			avg(b.io_read),
			avg(b.io_reread),
			avg(b.ran_read),
			avg(b.ran_write),
			avg(b.bkwd_read),
			avg(b.record_rewrite),
			avg(b.stride_read),
			avg(b.fwrite),
			avg(b.frewrite),
			avg(b.fread),
			avg(b.freread),
			a.batch_id,
			b.run_id,
			(b.end_time - b.start_time) as total_time,
			a.fs_version,
			a.kernel_version
		from 	fs_bench as a, iozone as b
		where 	a.id = b.run_id
		  and	a.bench_id in (%s)
		group by
			b.run_id,
			a.fs_type
		order by 
			b.run_id,
			a.batch_id,
			b.file_size,
			b.io_write desc""" % (max_bench_id);

#print "Query:", query;
cursor=db.cursor();
cursor.execute(query);
numrows = int(cursor.rowcount)
#print "Total Rows:", numrows;

data = {};
prev_run_id = -1;
for i in range(numrows):
	row = cursor.fetchone();

	host_name = row[0];
	fs_type = row[1];
	file_size = row[2];

	reclen = row[3];
	io_write = row[4];	
	io_rewrite = row[5];
	io_read = row[6];
	io_reread = row[7];
	ran_read = row[8];
	ran_write = row[9];
	bkwd_read = row[10];
	record_rewrite = row[11];
	stride_read = row[12];
	fwrite = row[13];
	frewrite = row[14];
	fread = row[15];
	freread = row[16];
	
	batch_id = row[17];
	if i == 0:
		batch_id_first_row = 0;
	elif prev_batch_id != batch_id:
		batch_id_first_row = i;

	run_id = row[18];
	total_time = row[19];
	try:
		first_total_time = data[batch_id_first_row]["total_time"];
	except:
		first_total_time = total_time;
	total_time_percent = first_total_time / total_time * 100;

	fs_version = row[20];
	kernel_version = row[21];
	
	#print "Initializing... Run ID:", run_id,"Batch ID:", batch_id,"I:",i;
	
	data[i] = 	{
					"run_id": row[18],
					"batch_id": row[17],
					"host_name": row[0],
					"fs_type": row[1],
					"file_size": row[2],
					"reclen": row[3],
					"io_write": row[4],
					"io_rewrite": row[5],
					"io_read": row[6],
					"io_reread": row[7],
					"ran_read": row[8],
					"ran_write": row[9],
					"bkwd_read": row[10],
					"record_rewrite": row[11],
					"stride_read": row[12],
					"fwrite": row[13],
					"frewrite": row[14],
					"fread": row[15],
					"freread": row[16],
					"total_time": row[19],
					"total_time_percent": total_time_percent,
					"fs_version": row[20],
					"kernel_version": row[21]
					}

	column_list = data[i].keys();
	try:
		column_values;
	except:
		column_values = {};
		
	for column in column_list:
		try:
			column_values[batch_id];
		except:
			column_values[batch_id] = {};
			
		try:
			column_values[batch_id][column];
		except:
			column_values[batch_id][column] = [];
		#Create list of all values in a column, so we know exactly which colors to make them.
		column_values[batch_id][column].append(eval(column));
		
		set_max_values(batch_id, column, eval(column), i);
		set_min_values(batch_id, column, eval(column), i);

	prev_run_id = run_id;
	prev_batch_id = batch_id;	

#pprint.pprint(column_values);
#pprint.pprint(data);
	
print """
<html>
<head>
<style type="text/css">
TD.header {text-align: center; backgroundcolor: "#CCFFFF" }
TD.rowheader {text-align: center; backgroundcolor: "#CCCFFF" }
TD.size {text-align: center; backgroundcolor: "#CCCFFF" }
TD.ksec {text-align: center; fontstyle: italic }
</style>
</head>
<body>
<div align="center"><b>IOZone Benchmarks</b></div>
<br>
<table align="center" border="3" cellpadding="2" cellspacing="1">
""";

def print_table_columns():
	print """
	<tr>
		<td align="center">FS</td>
		<td align="center">Size (KB)</td>
		<td align="center">Record Length</td>
		<td colspan="1" align="center">Write</td>
		<td colspan="1" align="center">Re-Write</td>
		<td colspan="1" align="center">Read</td>
		<td colspan="1" align="center">Re-Read</td>
		<td colspan="1" align="center">Random Read</td>
		<td colspan="1" align="center">Random Write</td>
		<td colspan="1" align="center">Read Backwards</td>
		<td colspan="1" align="center">Record Re-Write</td>
		<td colspan="1" align="center">Read Strided</td>
		<td colspan="1" align="center">F Write</td>
		<td colspan="1" align="center">F Re-Write</td>
		<td colspan="1" align="center">F Read</td>
		<td colspan="1" align="center">F Re-Read</td>
		<td rowspan="1" align="center">FS</td>
		<td rowspan="1" align="center">Total Run Time (secs)</td>
		<td rowspan="1" align="center">Total Run Time %</td>
		<td rowspan="1" align="center">Best case : Worst case</td>
		<td rowspan="1" align="center">FS Version</td>
		<td rowspan="1" align="center">Kernel Version</td>
	</tr>
	""";

print_table_columns();

prev_file_size = 0;
prev_run_id = -1;
for i in data:
	host_name = data[i]["host_name"];
	fs_type = data[i]["fs_type"];
	file_size = data[i]["file_size"];

	reclen = data[i]["reclen"];
	io_write = data[i]["io_write"];
	io_rewrite = data[i]["io_rewrite"];
	io_read = data[i]["io_read"];
	io_reread = data[i]["io_reread"];
	ran_read = data[i]["ran_read"];
	ran_write = data[i]["ran_write"];
	bkwd_read = data[i]["bkwd_read"];
	record_rewrite = data[i]["record_rewrite"];
	stride_read = data[i]["stride_read"];
	fwrite = data[i]["fwrite"];
	frewrite = data[i]["frewrite"];
	fread = data[i]["fread"];
	freread = data[i]["freread"];

	batch_id = data[i]["batch_id"];
	run_id = data[i]["run_id"];
	
	total_time = data[i]["total_time"];
	total_time_percent = data[i]["total_time_percent"];
	fs_version = data[i]["fs_version"];
	kernel_version = data[i]["kernel_version"];

	display_case_ratio = 0;
		
	if i > 0 and prev_batch_id != batch_id:
		print """
			<tr>
				<td colspan="99"><br></td>
			</tr>
		""";

		print_table_columns();
	
	print """<tr>""";

	columns = ["fs_type","file_size",
				"reclen","io_write","io_rewrite","io_read","io_reread","ran_read",
				"ran_write","bkwd_read","record_rewrite","stride_read","fwrite",
				"frewrite","fread","freread",
				"fs_type", "total_time", "total_time_percent", "display_case_ratio", "fs_version","kernel_version"];
	
	case_ratio = { "best": 0, "worst": 0};
	for column in columns:
		bgcolor = "#FFFFFF";
		max_cell = 0
		min_cell = 0;

		if column not in ["fs_type", "file_size", "reclen", "fs_version", "kernel_version", "display_case_ratio"]:
			try:
				#print "Row: %s Max Value Column Row: %s Column: %s" % (i,max_values[column]["row"], column);
				if re.match(".*cpu$",column) or column == "total_time":
					#If its the CPU column, swap colors.
					bgcolor = gen_bgcolor(	column_values[batch_id][column],
								eval(column),
								'descending' );
					if i in max_values[batch_id][column]["row"]:
						min_cell = 1;
				else:
					bgcolor = gen_bgcolor(	column_values[batch_id][column],
								eval(column),
								'ascending' );						
					if i in max_values[batch_id][column]["row"]:
						max_cell = 1;
			except:
				1;
				
			try:
				#print "Row: %s Min Value Column Row: %s" % (i,min_values[column]["row"]);
				if re.match(".*cpu$",column) or column == "total_time":
					#If its the CPU column, swap colors.
					bgcolor = gen_bgcolor(	column_values[batch_id][column],
								eval(column),
								'ascending' );
					if i in min_values[batch_id][column]["row"]:
						max_cell = 1;
				else:
					bgcolor = gen_bgcolor(	column_values[batch_id][column],
								eval(column),
								'descending' );
					if i in min_values[batch_id][column]["row"]:
						min_cell = 1;
			except:
				1;
				
		if min_cell == 1 and max_cell == 1:
			#print "Cell seems to be both min and max, ignoring both";
			bgcolor = "#FFFFFF";
		else:
			if min_cell == 1:
				case_ratio["worst"] += 1;
			elif max_cell == 1:
				case_ratio["best"] += 1;
				
		value = eval(column);
		
		if value > 0 and column == "eff_rating":
			value = "%0.4f" % value;
		elif value > 0 and column not in ["fs_type", "file_size", "num_files", "display_case_ratio", "fs_version", "kernel_version"]:
			value = "%0.1f" % value;
			
		if value <= 0:
			value = "<br>";
		
		if column == "display_case_ratio":
			print """			<td bgcolor="%s">%s:%s</td>""" % (bgcolor, case_ratio["best"], case_ratio["worst"]);
		else:
			print """			<td bgcolor="%s">%s</td>""" % (bgcolor, value);


	print """</tr>""";
	
	prev_file_size = file_size;
	prev_run_id = run_id;
	prev_batch_id = batch_id;

print """
<tr>
<td class="rowheader"><font size="+1"><b>Done..</b></font></td>
<td class="size">Done..</td>
<td>Done..</td>
</tr>
</table>
<br>
<br>
File System Benchmarking Report generated by <a href="http://fsbench.netnation.com">FS Bench</a> at %s
<br>
<br>
<div align="center">Copyright &copy; 2003 - <a href="http://www.netnation.com">NetNation Communications</a> / <a href="http://www.hostway.com">Hostway</a></div>
</body>
</html>
""" % (time.ctime());
