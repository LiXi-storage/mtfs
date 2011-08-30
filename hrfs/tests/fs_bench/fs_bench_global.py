# gen_bgcolor generates a "#rrggbb"-style strings suitable for
# use in a <td bgcolor=xxx> HTML tag.
def gen_bgcolor(value_list, value, sort_order):
	#print "gen_bgcolor(): Value: %s Sort: %s" % (value, sort_order);

	if value == '' or value == -1 or value == 0:
		return "#ffffff";
	
	#Only use unique values... so all duplicate entries should have the same color.
	value_list = unique(value_list);
	
	total = len(value_list);
	
	value_list.sort();

	index = value_list.index(value);
	#print "gen_bgcolor(): Index: %s" % (index);
	
	try:
		rank = float(index) / float(len(value_list));
	except:
		return "#ffffff";
	
	if sort_order == 'ascending':
		rank = 1.0 - rank;

	coffset = int(510.0 * rank);
	#print "gen_bgcolor(): Coffset: %s Rank: %s" % (coffset, rank);
	if rank > 0.5:
		coffset = 510 - coffset;
		color = "#%02xff%02x" % (coffset, coffset);
	else:
		color = "#ff%02x%02x" % (coffset, coffset);
	#print "gen_bgcolor(): Color: %s" % (color);
	
	#return [color, coffset];
	return color;

# gen_bgcolor generates a "#rrggbb"-style strings suitable for
# use in a <td bgcolor=xxx> HTML tag. Given a value in the
# range lo to hi, gen_bgcolor generates a color ranging from
# red on one extreme through white at the mean to green
# on the other extreme. With a dir value of "ascending",
# green is the low extreme; otherwise red is the low extreme.
"""
def gen_bgcolor(value, lo, hi, dir):
	range = float(hi) - float(lo);
	try:
		rank = ( value - lo ) / range;
	except:
	   return "#ffffff";
	if dir == 'ascending':
		rank = 1.0 - rank;
	coffset = int(512.0 * rank);
	if rank > 0.5:
		coffset = 512 - coffset;
		color = "#%02xff%02x" % (coffset, coffset);
	else:
		color = "#ff%02x%02x" % (coffset, coffset);
	return color;
"""
max_values = {};
def set_max_values(batch_id, column, value, row):
	global max_values;

	if value == -1:
		return 0;

	try:
		max_values[batch_id];
	except:
		max_values[batch_id] = {}
	try:
		max_values[batch_id][column];
	except:
		max_values[batch_id][column] = { "value": -1, "row": [] };
		#print "set_max_values(): Except... setting min vals for Column: %s" % (column,);

	if value > 0 and value > max_values[batch_id][column]["value"]:
		max_values[batch_id][column] = { "value": value, "row": [row] };
		#print "max_values(): FOUND MAX:  Column: %s Value: %s Row: %s -- Previous Max: %s " % (column, value, row, max_values[column]["value"]);
	elif value > 0 and value == max_values[batch_id][column]["value"]:
		max_values[batch_id][column]["row"].append(row);
		max_values[batch_id][column]["value"] = value;		
		#print "max_values(): FOUND DUP MAX:  Column: %s Value: %s Row: %s -- Previous Max: %s " % (column, value, row, max_values[column]["value"]);
		
	#print "max_values(): Column: %s Value: %s Row: %s" % (column, value, row);

min_values = {};
def set_min_values(batch_id, column, value, row):
	global min_values;

	if value == -1:
		return 0;
	
	try:
		min_values[batch_id];
	except:
		min_values[batch_id] = {};
	try:
		min_values[batch_id][column];
	except:
		min_values[batch_id][column] = { "value": value, "row": [] };
		#print "set_min_values(): Except... setting min vals";

	#if value != -1 and value != 0 and value <= min_values[column]["value"]:
	if value != -1 and value != 0 and value < min_values[batch_id][column]["value"]:
		min_values[batch_id][column] = { "value": value, "row": [row] };
		#print "min_values(): FOUND MIN:  Column: %s Value: %s Row: %s Previous Min: %s" % (column, value, row, min_values[column]["value"]);
	elif value != -1 and value != 0 and value == min_values[batch_id][column]["value"]:
		min_values[batch_id][column]["row"].append(row);
		min_values[batch_id][column]["value"] = value;
		#print "max_values(): FOUND DUP MIN:  Column: %s Value: %s Row: %s -- Previous Max: %s " % (column, value, row, max_values[column]["value"]);
		
	#print "min_values(): Column: %s Value: %s Row: %s" % (column, value, row);

def unique(s):
    """Return a list of the elements in s, but without duplicates.

    For example, unique([1,2,3,1,2,3]) is some permutation of [1,2,3],
    unique("abcabc") some permutation of ["a", "b", "c"], and
    unique(([1, 2], [2, 3], [1, 2])) some permutation of
    [[2, 3], [1, 2]].

    For best speed, all sequence elements should be hashable.  Then
    unique() will usually work in linear time.

    If not possible, the sequence elements should enjoy a total
    ordering, and if list(s).sort() doesn't raise TypeError it's
    assumed that they do enjoy a total ordering.  Then unique() will
    usually work in O(N*log2(N)) time.

    If that's not possible either, the sequence elements must support
    equality-testing.  Then unique() will usually work in quadratic
    time.
    """

    n = len(s)
    if n == 0:
        return []

    # Try using a dict first, as that's the fastest and will usually
    # work.  If it doesn't work, it will usually fail quickly, so it
    # usually doesn't cost much to *try* it.  It requires that all the
    # sequence elements be hashable, and support equality comparison.
    u = {}
    try:
        for x in s:
            u[x] = 1
    except TypeError:
        del u  # move on to the next method
    else:
        return u.keys()

    # We can't hash all the elements.  Second fastest is to sort,
    # which brings the equal elements together; then duplicates are
    # easy to weed out in a single pass.
    # NOTE:  Python's list.sort() was designed to be efficient in the
    # presence of many duplicate elements.  This isn't true of all
    # sort functions in all languages or libraries, so this approach
    # is more effective in Python than it may be elsewhere.
    try:
        t = list(s)
        t.sort()
    except TypeError:
        del t  # move on to the next method
    else:
        assert n > 0
        last = t[0]
        lasti = i = 1
        while i < n:
            if t[i] != last:
                t[lasti] = last = t[i]
                lasti += 1
            i += 1
        return t[:lasti]

    # Brute force is all that's left.
    u = []
    for x in s:
        if x not in u:
            u.append(x)
    return u