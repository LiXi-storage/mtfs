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

query = """select distinct(bench_id) from fs_bench where bench_program = 'bonnie' order by bench_id desc limit 2""";
cursor=db.cursor();
cursor.execute(query);
numrows = int(cursor.rowcount)

bench_ids = [];
for i in range(numrows):
	bench_ids.append(str(cursor.fetchone()[0]));
bench_ids_sql = ",".join(bench_ids);
#bench_ids_sql = "149,143,131";

query = """	select
			a.host_name,
			a.fs_type,
			b.file_size,
			avg(b.putc) as putc,
			avg(b.putc_cpu) as putc_cpu,
			avg(b.put_block) as put_block,
			avg(b.put_block_cpu) as put_block_cpu,
			avg(b.rewrite),
			avg(b.rewrite_cpu),
			avg(b.getc),
			avg(b.getc_cpu),
			avg(b.get_block),
			avg(b.get_block_cpu),
			avg(b.seeks),
			avg(b.seeks_cpu),
			avg(b.num_files),
			avg(b.seq_create),
			avg(b.seq_create_cpu),
			avg(b.seq_stat),
			avg(b.seq_stat_cpu),
			avg(b.seq_del),
			avg(b.seq_del_cpu),
			avg(b.ran_create),
			avg(b.ran_create_cpu),
			avg(b.ran_stat),
			avg(b.ran_stat_cpu),
			avg(b.ran_del),
			avg(b.ran_del_cpu),
			a.batch_id,
			b.run_id,
			(b.end_time - b.start_time) as total_time,
			a.fs_version,
			a.kernel_version
		from 	fs_bench as a, bonnie as b
		where 	a.id = b.run_id
			AND a.bench_id in (%s)
		group by
			b.run_id,
			a.fs_type
		order by
			b.file_size,
			b.num_files,
			a.fs_type,
			a.kernel_version""" % (bench_ids_sql);

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
	putc = row[3];
	putc_cpu = row[4];
	if putc > 0 and putc_cpu > 0:
		putc_wrk_per_unit = putc / putc_cpu;
	else:
		putc_wrk_per_unit = 0;
	
	put_block = row[5];
	put_block_cpu = row[6];
	put_block_wrk_per_unit = put_block / put_block_cpu;
	
	rewrite = row[7];
	rewrite_cpu = row[8];
	rewrite_wrk_per_unit = rewrite / rewrite_cpu;
	
	getc = row[9];
	getc_cpu = row[10];
	if getc > 0 and getc_cpu > 0:
		getc_wrk_per_unit = getc / getc_cpu;
	else:
		getc_wrk_per_unit = 0;
	
	get_block = row[11];
	get_block_cpu = row[12];
	get_block_wrk_per_unit = get_block / get_block_cpu;
	
	seeks = row[13];
	seeks_cpu = row[14];
	#CPU usage should never be 0... so lets make it the next best thing instead.
	if seeks_cpu == 0:
		seeks_cpu = 0.1;
		
	if seeks > 0 and seeks_cpu > 0:
		seeks_wrk_per_unit = seeks / seeks_cpu;
	else:
		seeks_wrk_per_unit = 0;
		
	num_files = row[15];
	
	seq_create = row[16];
	seq_create_cpu = row[17];
	seq_create_wrk_per_unit = seq_create / seq_create_cpu;
	
	seq_stat = row[18];
	seq_stat_cpu = row[19];
	if seq_stat > 0 and seq_stat_cpu > 0:
		seq_stat_wrk_per_unit = seq_stat / seq_stat_cpu;
	else:
		seq_stat_wrk_per_unit = 0;
	
	seq_del = row[20];
	seq_del_cpu = row[21];
	seq_del_wrk_per_unit = seq_del / seq_del_cpu;
	
	ran_create = row[22];
	ran_create_cpu = row[23];
	if ran_create > 0 and ran_create_cpu > 0:
		ran_create_wrk_per_unit = ran_create / ran_create_cpu;
	else:
		ran_create_wrk_per_unit = 0;	

	ran_stat = row[24];
	ran_stat_cpu = row[25];
	if ran_stat_cpu == 0:
		ran_stat_cpu = 0.1;

	if ran_stat > 0 and ran_stat_cpu > 0:
		ran_stat_wrk_per_unit = ran_stat / ran_stat_cpu;
	else:
		ran_stat_wrk_per_unit = 0;
	
	ran_del = row[26];
	ran_del_cpu = row[27];
	#CPU usage should never be 0... so lets make it the next best thing instead.
	if ran_del_cpu == 0:
		ran_del_cpu = 0.1;

	if ran_del > 0 and ran_del_cpu > 0:
		ran_del_wrk_per_unit = ran_del / ran_del_cpu;
	else:
		ran_del_wrk_per_unit = 0;
	
	batch_id = row[28];

	if i == 0:
		batch_id_first_row = 0;
	elif prev_batch_id != batch_id:
		batch_id_first_row = i;
	#print "Batch ID first row: %s" % (batch_id_first_row);
	
	run_id = row[29];
	total_time = row[30];
	try:
		first_total_time = data[batch_id_first_row]["total_time"];
	except:
		first_total_time = total_time;
	total_time_percent = first_total_time / total_time * 100;

	fs_version = row[31];
	kernel_version = row[32];

	total_wrk_per_unit =	putc_wrk_per_unit + put_block_wrk_per_unit + rewrite_wrk_per_unit + getc_wrk_per_unit + get_block_wrk_per_unit +seeks_wrk_per_unit + seq_create_wrk_per_unit + seq_stat_wrk_per_unit + seq_del_wrk_per_unit + ran_create_wrk_per_unit + ran_stat_wrk_per_unit + ran_del_wrk_per_unit;
	avg_wrk_per_unit = total_wrk_per_unit / 12;
	try:
		first_avg_wrk_per_unit = data[batch_id_first_row]["avg_wrk_per_unit"];
	except:
		first_avg_wrk_per_unit = avg_wrk_per_unit;
	avg_wrk_per_unit_percent = avg_wrk_per_unit / first_avg_wrk_per_unit * 100;
	
	eff_rating = avg_wrk_per_unit / total_time;
	
	#print "Initializing... Run ID:", run_id,"Batch ID:", batch_id,"I:",i;
	
	data[i] = 	{
					"run_id": row[29],
					"batch_id": row[28],
					"host_name": row[0],
					"fs_type": row[1],
					"file_size": row[2],
					"putc": row[3],
					"putc_cpu": row[4],
					"putc_wrk_per_unit": putc_wrk_per_unit,
					"put_block": row[5],
					"put_block_cpu": row[6],
					"put_block_wrk_per_unit": put_block_wrk_per_unit,
					"rewrite": row[7],
					"rewrite_cpu": row[8],
					"rewrite_wrk_per_unit": rewrite_wrk_per_unit,
					"getc": row[9],
					"getc_cpu": row[10],
					"getc_wrk_per_unit": getc_wrk_per_unit,
					"get_block": row[11],
					"get_block_cpu": row[12],
					"get_block_wrk_per_unit": get_block_wrk_per_unit,
					"seeks": row[13],
					"seeks_cpu": seeks_cpu,
					"seeks_wrk_per_unit": seeks_wrk_per_unit,
					"num_files": row[15],
					"seq_create": row[16],
					"seq_create_cpu": row[17],
					"seq_create_wrk_per_unit": seq_create_wrk_per_unit,
					"seq_stat": row[18],
					"seq_stat_cpu": row[19],
					"seq_stat_wrk_per_unit": seq_stat_wrk_per_unit,
					"seq_del": row[20],
					"seq_del_cpu": row[21],
					"seq_del_wrk_per_unit": seq_del_wrk_per_unit,
					"ran_create": row[22],
					"ran_create_cpu": ran_create_cpu,
					"ran_create_wrk_per_unit": ran_create_wrk_per_unit,
					"ran_stat": row[24],
					"ran_stat_cpu": ran_stat_cpu,
					"ran_stat_wrk_per_unit": ran_stat_wrk_per_unit,
					"ran_del": row[26],
					"ran_del_cpu": ran_del_cpu,
					"ran_del_wrk_per_unit": ran_del_wrk_per_unit,
					"total_wrk_per_unit": total_wrk_per_unit,
					"avg_wrk_per_unit": avg_wrk_per_unit,
					"avg_wrk_per_unit_percent": avg_wrk_per_unit_percent,
					"eff_rating": eff_rating,
					"total_time": row[30],
					"total_time_percent": total_time_percent,
					"fs_version": row[31],
					"kernel_version": row[32]
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

	#pprint.pprint(max_values);
	
	prev_run_id = run_id;
	prev_batch_id = batch_id;	

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
<div align="center"><b>Bonnie++ Benchmarks</b></div>
<br>
<table align="center" border="3" cellpadding="2" cellspacing="1">
""";

def print_table_columns():
	print """
	<tr>
		<td colspan="3" class="header"><br></td>
		<td colspan="9" class="header"><font size="+2"><b>Sequential Output</b></font></td>
		<td colspan="6" class="header"><font size="+2"><b>Sequential Input</b></font></td>
		<td colspan="3" rowspan="2" class="header"><font size="+2"><b>Random<br>Seeks</b></font></td>
		<td colspan="1" class="header"><br></td>
		<td colspan="9" class="header"><font size="+2"><b>Sequential Create</b></font></td>
		<td colspan="9" class="header"><font size="+2"><b>Random Create</b></font></td>
		<td colspan="1" class="header"><br></td>
		<td colspan="8" class="header"><font size="+2"><b>Other</b></font></td>
	</tr>
	<tr>
		<td rowspan="2" align="center">FS</td>
		<td rowspan="2" align="center">Size:Chunk Size</td>
		<td rowspan="2" align="center">Num Files (*1000)</td>
		<td colspan="3" align="center">Per Char</td>
		<td colspan="3" align="center">Block</td>
		<td colspan="3" align="center">Rewrite</td>
		<td colspan="3" align="center">Per Char</td>
		<td colspan="3" align="center">Block</td>
		<td rowspan="2" align="center">FS</td>
		<td colspan="3" align="center">Create</td>
		<td colspan="3" align="center">Read</td>
		<td colspan="3" align="center">Delete</td>
		<td colspan="3" align="center">Create</td>
		<td colspan="3" align="center">Read</td>
		<td colspan="3" align="center">Delete</td>
		<td rowspan="2" align="center">FS</td>
		<td rowspan="2" align="center">Avg Work / CPU</td>
		<td rowspan="2" align="center">Avg Work / CPU %</td>
		<td rowspan="2" align="center">Total Run Time (secs)</td>
		<td rowspan="2" align="center">Total Run Time %</td>
		<td rowspan="2" align="center">Overall Rating</td>
		<td rowspan="2" align="center">Best case : Worst case</td>
		<td rowspan="2" align="center">FS Version</td>
		<td rowspan="2" align="center">Kernel Version</td>
	</tr>
	<tr>
		<td class="ksec"><font size="-2">K/sec</font></td>
		<td class="ksec"><font size="-2">% CPU</font></td>
		<td class="ksec"><font size="-2">Work / CPU</font></td>
		
		<td class="ksec"><font size="-2">K/sec</font></td>
		<td class="ksec"><font size="-2">% CPU</font></td>
		<td class="ksec"><font size="-2">Work / CPU</font></td>
		
		<td class="ksec"><font size="-2">K/sec</font></td>
		<td class="ksec"><font size="-2">% CPU</font></td>
		<td class="ksec"><font size="-2">Work / CPU</font></td>
		
		<td class="ksec"><font size="-2">K/sec</font></td>
		<td class="ksec"><font size="-2">% CPU</font></td>
		<td class="ksec"><font size="-2">Work / CPU</font></td>
		
		<td class="ksec"><font size="-2">K/sec</font></td>
		<td class="ksec"><font size="-2">% CPU</font></td>
		<td class="ksec"><font size="-2">Work / CPU</font></td>
		
		<td class="ksec"><font size="-2">K/sec</font></td>
		<td class="ksec"><font size="-2">% CPU</font></td>
		<td class="ksec"><font size="-2">Work / CPU</font></td>
		
		<td class="ksec"><font size="-2">/ sec</font></td>
		<td class="ksec"><font size="-2">% CPU</font></td>
		<td class="ksec"><font size="-2">Work / CPU</font></td>
		
		<td class="ksec"><font size="-2">/ sec</font></td>
		<td class="ksec"><font size="-2">% CPU</font></td>
		<td class="ksec"><font size="-2">Work / CPU</font></td>
		
		<td class="ksec"><font size="-2">/ sec</font></td>
		<td class="ksec"><font size="-2">% CPU</font></td>
		<td class="ksec"><font size="-2">Work / CPU</font></td>
		
		<td class="ksec"><font size="-2">/ sec</font></td>
		<td class="ksec"><font size="-2">% CPU</font></td>
		<td class="ksec"><font size="-2">Work / CPU</font></td>
		
		<td class="ksec"><font size="-2">/ sec</font></td>
		<td class="ksec"><font size="-2">% CPU</font></td>
		<td class="ksec"><font size="-2">Work / CPU</font></td>
		
		<td class="ksec"><font size="-2">/ sec</font></td>
		<td class="ksec"><font size="-2">% CPU</font></td>
		<td class="ksec"><font size="-2">Work / CPU</font></td>
	</tr>	
	""";

print_table_columns();

#pprint.pprint(max_values);
#pprint.pprint(min_values);
	
prev_file_size = 0;
prev_run_id = -1;
for i in data:
	#row = cursor.fetchone();

	host_name = data[i]["host_name"];
	fs_type = data[i]["fs_type"];
	file_size = data[i]["file_size"];
	putc = data[i]["putc"];
	putc_cpu = data[i]["putc_cpu"];
	putc_wrk_per_unit = data[i]["putc_wrk_per_unit"];
	
	put_block = data[i]["put_block"];
	put_block_cpu = data[i]["put_block_cpu"];
	put_block_wrk_per_unit = data[i]["put_block_wrk_per_unit"];
	
	rewrite = data[i]["rewrite"];
	rewrite_cpu = data[i]["rewrite_cpu"];
	rewrite_wrk_per_unit = data[i]["rewrite_wrk_per_unit"];
	
	getc = data[i]["getc"];
	getc_cpu = data[i]["getc_cpu"];
	getc_wrk_per_unit = data[i]["getc_wrk_per_unit"];
	
	get_block = data[i]["get_block"];
	get_block_cpu = data[i]["get_block_cpu"];
	get_block_wrk_per_unit = data[i]["get_block_wrk_per_unit"];
	
	seeks = data[i]["seeks"];
	seeks_cpu = data[i]["seeks_cpu"];
	seeks_wrk_per_unit = data[i]["seeks_wrk_per_unit"];
	
	num_files = data[i]["num_files"];
	
	seq_create = data[i]["seq_create"];
	seq_create_cpu = data[i]["seq_create_cpu"];
	seq_create_wrk_per_unit = data[i]["seq_create_wrk_per_unit"];
	
	seq_stat = data[i]["seq_stat"];
	seq_stat_cpu = data[i]["seq_stat_cpu"];
	seq_stat_wrk_per_unit = data[i]["seq_stat_wrk_per_unit"];
	
	seq_del = data[i]["seq_del"];
	seq_del_cpu = data[i]["seq_del_cpu"];
	seq_del_wrk_per_unit = data[i]["seq_del_wrk_per_unit"];
	
	ran_create = data[i]["ran_create"];
	ran_create_cpu = data[i]["ran_create_cpu"];
	ran_create_wrk_per_unit = data[i]["ran_create_wrk_per_unit"];
		
	ran_stat = data[i]["ran_stat"];
	ran_stat_cpu = data[i]["ran_stat_cpu"];
	ran_stat_wrk_per_unit = data[i]["ran_stat_wrk_per_unit"];
	
	ran_del = data[i]["ran_del"];
	ran_del_cpu = data[i]["ran_del_cpu"];
	ran_del_wrk_per_unit = data[i]["ran_del_wrk_per_unit"];

	batch_id = data[i]["batch_id"];
	run_id = data[i]["run_id"];
	
	total_time = data[i]["total_time"];
	total_time_percent = data[i]["total_time_percent"];
	fs_version = data[i]["fs_version"];
	kernel_version = data[i]["kernel_version"];

	display_case_ratio = 0;
	
	avg_wrk_per_unit = data[i]["avg_wrk_per_unit"];
	avg_wrk_per_unit_percent = data[i]["avg_wrk_per_unit_percent"];
	eff_rating = data[i]["eff_rating"];
	
	if i > 0 and prev_batch_id != batch_id:
		print """
			<tr>
				<td colspan="99"><br></td>
			</tr>
		""";

		print_table_columns();
	
	print """<tr>""";

	columns = [	"fs_type","file_size","num_files",
			"putc","putc_cpu","putc_wrk_per_unit",
			"put_block","put_block_cpu","put_block_wrk_per_unit",
			"rewrite","rewrite_cpu","rewrite_wrk_per_unit",
			"getc","getc_cpu","getc_wrk_per_unit",
			"get_block","get_block_cpu","get_block_wrk_per_unit",
			"seeks","seeks_cpu","seeks_wrk_per_unit",
			"fs_type",
			"seq_create","seq_create_cpu","seq_create_wrk_per_unit",
			"seq_stat","seq_stat_cpu","seq_stat_wrk_per_unit",
			"seq_del","seq_del_cpu","seq_del_wrk_per_unit",
			"ran_create","ran_create_cpu","ran_create_wrk_per_unit",
			"ran_stat","ran_stat_cpu","ran_stat_wrk_per_unit",
			"ran_del","ran_del_cpu","ran_del_wrk_per_unit",
			"fs_type", "avg_wrk_per_unit","avg_wrk_per_unit_percent","total_time", "total_time_percent", "eff_rating", "display_case_ratio", "fs_version","kernel_version"];
	
	case_ratio = { "best": 0, "worst": 0};
	for column in columns:
		bgcolor = "#FFFFFF";
		max_cell = 0
		min_cell = 0;
		
		#if column != "fs_type" and column != "file_size" and column != "num_files" and column != "fs_version" and column != "kernel_version":
		if column not in ["fs_type", "file_size", "num_files", "fs_version", "kernel_version"]:
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
