#!/bin/sh
#
# Copyright (C) 2011 Li Xi <pkuelelixi@163.com>
#

desc="tests for rule_tree"

dir=`dirname $0`
. ${dir}/../misc.sh

echo "1..34"

#
# TEST FORMAT:
#


# test 1
# cd .. at /
IN='workspace.cd("..") workspace.ls()
' OUT='
.. (graph)
types (graph)
' expect 0

# test 2
# new a existed component should fail
IN='workspace.new({name="sub1"}) workspace.new({name="sub1"})
workspace.ls("sub1")
' OUT='
sub1: type=NULL, pins=NULL, is_graph
' expect 0

# test 3
# remove a existed component
IN='
workspace.new({name="sub1"})
workspace.rm("sub1")
workspace.ls()
' OUT='
.. (graph)
types (graph)
' expect 0

# test 4
# remove a nonexisted component
IN='
workspace.rm({name="sub1"}) workspace.rm("sub1")
workspace.ls()
' OUT='
.. (graph)
types (graph)
' expect 0

# test 5
# recursively remove a component
IN='
workspace.new({name="sub1"})
workspace.cd("sub1")
workspace.new({name="sub2_1"}) workspace.new({name="sub2_2"}) workspace.new({name="sub2_3"})
workspace.cd("sub2_1")
workspace.new({name="sub3_1"})
workspace.cd("..") workspace.cd("..")
workspace.rm("sub1")
workspace.ls()
' OUT='
.. (graph)
types (graph)
' expect 0

# test 6
# declare component types
IN='
workspace.declare({type="type1", out_num=10, selector=function () print "aoao" end})
workspace.declare({type="type2", out_num=10, selector=function () print "aoao" end})
workspace.declare({type="type1", out_num=10, selector=function () print "aoao" end})
workspace.declare({type="type2", out_num=10, selector=function () print "aoao" end})
workspace.type();
' OUT='
.. (graph)
type2 (filter)
type1 (filter)
' expect 0

# test 7
# new a component with type
IN='
workspace.declare({type="type1", out_num=10, selector=function () print "aoao" end})
workspace.new({type="type1", name="sub1"})
workspace.ls()
' OUT='
.. (graph)
sub1 (filter)
types (graph)
' expect 0

# test 8
# new a existed component with type
IN='
workspace.declare({type="type1", out_num=10, selector=function () print "aoao" end})
workspace.new({type="type1", name="sub1"})
workspace.new({type="type1", name="sub1"})
workspace.ls()
' OUT='
.. (graph)
sub1 (filter)
types (graph)
' expect 0

# test 9
# cp a component with type
IN='
workspace.declare({type="type1", out_num=10, selector=function () print "aoao" end})
workspace.new({type="type1", name="sub1_1"})
workspace.cp("sub1_1", "sub1_2")
workspace.ls()
' OUT='
.. (graph)
sub1_2 (filter)
sub1_1 (filter)
types (graph)
' expect 0

# test 10
# cp a component without type
IN='
workspace.new({name="sub1_1"})
workspace.cp("sub1_1", "sub1_2")
workspace.ls()
' OUT='
.. (graph)
sub1_2 (graph)
sub1_1 (graph)
types (graph)
' expect 0

# test 11
# recursively cp a component
IN='
workspace.declare({type="type1", out_num=10, selector=function () print "aoao" end})

workspace.new({name="sub1_1"})

workspace.cd("sub1_1")
workspace.new({name="sub2_1"})
workspace.new({type="type1", name="sub2_2"})
workspace.new({type="type1", name="sub2_3"})

workspace.cd("sub2_1")
workspace.new({name="sub3_1"})

workspace.cd("..") workspace.cd("..")
workspace.cp("sub1_1", "sub1_2")

print ("components under /:\n")
workspace.ls()
print ("components under /sub1_2:\n")
workspace.cd("sub1_2") workspace.ls()
print ("components under /sub1_2/sub2_1:\n")
workspace.cd("sub2_1") workspace.ls()
' OUT='
components under /:
.. (graph)
sub1_2 (graph)
sub1_1 (graph)
types (graph)
components under /sub1_2:
.. (graph)
sub2_1 (graph)
sub2_2 (filter)
sub2_3 (filter)
components under /sub1_2/sub2_1:
.. (graph)
sub3_1 (graph)
' expect 0

# test 12
# declare error capture
IN='
-- no table given
workspace.declare(1234)
-- error type given
workspace.declare({type=type, out_num=10, selector=function () print "aoao" end})
-- zero out_num given
workspace.declare({type="type1", out_num=0, selector=function () print "aoao" end})
-- negative out_num given
workspace.declare({type="type2", out_num=-1, selector=function () print "aoao" end})
-- error selector given
workspace.declare({type="type3", out_num=1, selector=function () print "aoao" end})
-- too many outpin_names given
workspace.declare({type="type4", out_num=1, selector=function () print "aoao" end, outpin_names={"out1", "out2"}})
-- error outpin_names given
workspace.declare({type="type5", out_num=2, selector=function () print "aoao" end, outpin_names={"out1", type}})
-- error inpin_names given
workspace.declare({type="type5", out_num=2, selector=function () print "aoao" end, inpin_name=type})
' OUT='
' expect 0

# test 13
# declare a type without pin name setted
IN='
workspace.declare({type="type1", out_num=2, selector=function () print "aoao" end})
workspace.type("type1");
' OUT='
type1: type="type1", pins={"in_0", "out_0", "out_1"}, is_filter
' expect 0

# test 14
# declare a type with pin name setted
IN='
workspace.declare({type="type1", out_num=2, selector=function () print "aoao" end, inpin_name="in0", outpin_names={"out0", "out1"}})
workspace.type("type1");
' OUT='
type1: type="type1", pins={"in0", "out0", "out1"}, is_filter
' expect 0


# test 15
# declare and ls some types with pin name setted
IN='
workspace.declare({type="type1", out_num=2, selector=function () print "aoao" end, inpin_name="in0", outpin_names={"out0", "out1"}})
workspace.declare({type="type2", out_num=2, selector=function () print "aoao" end, inpin_name="in0", outpin_names={"out0", "out1"}})
workspace.declare({type="type3", out_num=2, selector=function () print "aoao" end, inpin_name="in0", outpin_names={"out0", "out1"}})
workspace.type("type1", "type2", "type3");
' OUT='
type1: type="type1", pins={"in0", "out0", "out1"}, is_filter
type2: type="type2", pins={"in0", "out0", "out1"}, is_filter
type3: type="type3", pins={"in0", "out0", "out1"}, is_filter
' expect 0

# test 16
# declare and ls some instances
IN='
workspace.declare({type="type1", out_num=2, selector=function () print "aoao" end, inpin_name="in0", outpin_names={"out0", "out1"}})
workspace.new({name="sub1"})
workspace.new({type="type1", name="sub2"})
workspace.new({type="type1", name="sub3"})
workspace.ls("sub1", "sub2", "sub3");
' OUT='
sub1: type=NULL, pins=NULL, is_graph
sub2: type="type1", pins={"in0", "out0", "out1"}, is_filter
sub3: type="type1", pins={"in0", "out0", "out1"}, is_filter
' expect 0

# test 17
# ls a nonexist instance
IN='
workspace.declare({type="type1", out_num=2, selector=function () print "aoao" end, inpin_name="in0", outpin_names={"out0", "out1"}})
workspace.new({name="sub1"})
workspace.new({type="type1", name="sub2"})
workspace.ls();
' OUT='
.. (graph)
sub2 (filter)
sub1 (graph)
types (graph)
' expect 0

# test 18
# pwd tests
IN='
workspace.pwd()
workspace.new({name="sub1"})
workspace.cd("sub1")
workspace.pwd()
workspace.new({name="sub2"})
workspace.cd("sub2")
workspace.pwd()
workspace.new({name="sub3"})
workspace.cd("sub3")
workspace.pwd()
workspace.cd("..")
workspace.pwd()
workspace.cd("..")
workspace.pwd()
workspace.cd("..")
workspace.pwd()
' OUT='
/
/sub1
/sub1/sub2
/sub1/sub2/sub3
/sub1/sub2
/sub1
/
' expect 0

# test 19
# pwd with error argument
IN='
workspace.pwd("sub1")
' OUT='
' expect 0

# test 20
# cd to nonexisted subcomponent
IN='
workspace.new({name="sub2"})
workspace.cd("sub1")
workspace.pwd()
' OUT='
/
' expect 0

# test 21
# cd with error argument
IN='
workspace.new({name="sub2"})
workspace.cd(type)
workspace.pwd()
' OUT='
/
' expect 0

# test 22
# cd with too many argument
IN='
workspace.new({name="sub1"})
workspace.new({name="sub2"})
workspace.cd("sub2", "sub1")
workspace.pwd()
' OUT='
/
' expect 0

# test 23
# cd with too few argument
IN='
workspace.new({name="sub1"})
workspace.new({name="sub2"})
workspace.cd()
workspace.pwd()
' OUT='
/
' expect 0

# test 24
# rm some components
IN='
workspace.new({name="sub1"})
workspace.new({name="sub2"})
workspace.new({name="sub3"})
workspace.rm("sub1", "sub2", "sub3")
workspace.ls()
' OUT='
.. (graph)
types (graph)
' expect 0

# test 25
# cp nonexist components
IN='
workspace.new({name="sub1"})
workspace.cp("sub2", "sub3")
workspace.ls()
' OUT='
.. (graph)
sub1 (graph)
types (graph)
' expect 0

# test 26
# cp with too few argument
IN='
workspace.new({name="sub1"})
workspace.cp("sub1")
workspace.ls()
' OUT='
.. (graph)
sub1 (graph)
types (graph)
' expect 0

# test 27
# cp with too many argument
IN='
workspace.new({name="sub1"})
workspace.cp("sub1", "sub2", "sub3")
workspace.ls()
' OUT='
.. (graph)
sub1 (graph)
types (graph)
' expect 0

# test 28
# dump some components
IN='
workspace.new({name="sub1"})
workspace.new({name="sub2"})
workspace.new({name="sub3"})
workspace.dump()
' OUT='
digraph G {
	sub3 [label= "sub3"];
	sub2 [label= "sub2"];
	sub1 [label= "sub1"];
	types [label= "types"];
}
' expect 0

# test 29
# dump some components
IN='
workspace.declare({type="type1", out_num=2, selector=function () print "aoao" end, inpin_name="in0", outpin_names={"out0", "out1"}})
workspace.new({type="type1", name="sub1"})
workspace.new({type="type1", name="sub2"})
workspace.new({type="type1", name="sub3"})
workspace.link("sub1", "out0", "sub2", "in0")
workspace.link("sub1", "out1", "sub2", "in0")
workspace.link("sub2", "out0", "sub3", "in0")
workspace.link("sub2", "out1", "sub3", "in0")
workspace.dump()
' OUT='
digraph G {
	sub3 [label= "sub3"];
	sub2 [label= "sub2"];
	sub1 [label= "sub1"];
	types [label= "types"];
	sub2->sub3;
	sub2->sub3;
	sub1->sub2;
	sub1->sub2;
}
' expect 0

# test 30
# linking component to itself should fail
IN='
workspace.declare({type="type1", out_num=2, selector=function () print "aoao" end, inpin_name="in0", outpin_names={"out0", "out1"}})
workspace.new({type="type1", name="sub1"})
workspace.link("sub1", "out0", "sub1", "in0")
workspace.dump()
' OUT='
digraph G {
	sub1 [label= "sub1"];
	types [label= "types"];
}
' expect 0

# test 31
# declaring component with pins having the same name should fail
IN='
workspace.declare({type="type1", out_num=2, selector=function () print "aoao" end, inpin_name="in0", outpin_names={"out0", "out0"}})
workspace.type();
' OUT='
.. (graph)
' expect 0

# test 32
# packing component
IN='
workspace.declare({type="type1", out_num=2, selector=function () print "aoao" end, inpin_name="in0", outpin_names={"out0", "out1"}})
workspace.new({name="sub1"})
workspace.cd("sub1")
workspace.new({type="type1", name="sub2"})
workspace.cd("..")
workspace.pack({name="sub1", pins={{sub="sub2", pin="in0", name="in0"}, {sub="sub2", pin="out0", name="out0"}, {sub="sub2", pin="out1", name="out1"}}})
workspace.ls("sub1");
' OUT='
sub1: type=NULL, pins={"in0", "out0", "out1"}, is_graph
' expect 0

# test 33
# packing component with pins having the same name should fail
IN='
workspace.declare({type="type1", out_num=2, selector=function () print "aoao" end, inpin_name="in0", outpin_names={"out0", "out1"}})
workspace.new({name="sub1"})
workspace.cd("sub1")
workspace.new({type="type1", name="sub2"})
workspace.cd("..")
workspace.pack({name="sub1", pins={{sub="sub2", pin="in0", name="in0"}, {sub="sub2", pin="out0", name="out0"}, {sub="sub2", pin="out1", name="out0"}}})
workspace.ls("sub1");
' OUT='
sub1: type=NULL, pins=NULL, is_graph
' expect 0

# test 34
# declareing packed component
IN='
workspace.declare({type="type1", out_num=2, selector=function () print "aoao" end, inpin_name="in0", outpin_names={"out0", "out1"}})
workspace.new({name="sub1"})
workspace.cd("sub1")
workspace.new({type="type1", name="sub2"})
workspace.cd("..")
workspace.pack({name="sub1", pins={{sub="sub2", pin="in0", name="in0"}, {sub="sub2", pin="out0", name="out0"}, {sub="sub2", pin="out1", name="out1"}}})
workspace.declare({name="sub1", type="type2"})
workspace.new({type="type2", name="sub1_1"})
workspace.ls("sub1_1")
' OUT='
sub1_1: type="type2", pins={"in0", "out0", "out1"}, is_graph
' expect 0

