#!/usr/bin/python

import sys, os, inspect

old_versions = [ "1.0", "1.1", "2.0", "2.1", "3.0", "3.1", "3.2" ]
versions = [ "4.0", "4.1", "5.0", "5.1", "5.2", "6.0", "6.1", "6.2", "6.3", "7.0", "8.0", "9.0", "10.0" ]

# These ranges have been marked as reserved with double width characters. Thus it is acceptable for these
# not to appear in the DerivedAge list.
reserved_double_width_ranges = [(0x4DB6, 0x4DBF), (0x9FCC, 0x9FFF), (0xFA6E, 0xFA6E), (0xFA6F, 0xFA6F),
	(0xFADA, 0xFAFF), (0x2A6D7, 0x2F7FF), (0x2FA1E, 0x2FFFD), (0x30000, 0x3FFFD)]

# These ranges are used in several libraries and e.g. xterm to determine the width of characters. They do
# not correctly correspond to a particular unicode version, as they include codepoints which have not been
# assigned yet. They should be avoided for testing unicode capabilities.
double_width_test_exclude_ranges = [(0x1100, 0x115f), (0x2329, 0x2329), (0x232a, 0x232a), (0x2e80, 0x303e),
	(0x3040, 0xa4cf), (0xac00, 0xd7a3), (0xf900, 0xfaff), (0xfe10, 0xfe19), (0xfe30, 0xfe6f), (0xff00, 0xff60),
	(0xffe0, 0xffe6), (0x20000, 0x2fffd), (0x30000, 0x3fffd)]

def main():
	if len(sys.argv) != 4:
		sys.stderr.write("Usage: generate_chardata.py <UnicodeData file> <EastAsianWidth file> <DerivedAge file>\n");
		sys.exit(1)

	available_since = [0xff] * 0x110000

	derived_age = open(sys.argv[3], "r")
	for line in derived_age:
		line = line.strip()
		if len(line) == 0 or line.startswith('#'):
			continue
		range, _ , version = line.partition(';')
		version, _, _ = version.partition('#')
		version = version.strip()
		range_start, _, range_end = range.partition("..")
		range_start = int(range_start, 16)
		range_end = int(range_end, 16) if range_end != '' else range_start
		if version in old_versions:
			value = 0
		elif version in versions:
			value = versions.index(version)
		else:
			print >>sys.stderr, "Version {0} has not been assigned an index yet".format(version)
			sys.exit(1)

		for i in xrange(range_start, range_end + 1):
			available_since[i] = value

	cell_width = [0xff] * 0x110000
	east_asian_width = open(sys.argv[2], "r")
	for line in east_asian_width:
		line = line.strip()
		if len(line) == 0 or line.startswith('#'):
			continue
		range, _ , width = line.partition(';')
		width, _, _ = width.partition('#')
		width = width.strip()
		range_start, _, range_end = range.partition('..')
		range_start = int(range_start, 16)
		range_end = int(range_end, 16) if range_end != '' else range_start
		if width != 'F' and width != 'W':
			continue

		for i in xrange(range_start, range_end + 1):
			cell_width[i] = 2
	east_asian_width.close()

	unicode_data = open(sys.argv[1], "r")
	combining_characters = set()
	name_mapping = {}
	for line in unicode_data:
		line = line.strip()
		if len(line) == 0 or line.startswith('#'):
			continue
		parts = line.split(';')
		cp = int(parts[0], 16)
		name_mapping[cp] = parts[1]
		if parts[4] == 'NSM' or parts[2] == 'Cf':
			cell_width[cp] = 0
		elif parts[2] == 'Cc':
			cell_width[cp] = -1
		if int(parts[3]) > 0:
			combining_characters.add(cp)

	# Add in some hacks which don't appear as data in the source files.
	cell_width[0x00ad] = 1

	for i in xrange(0, 0x110000):
		if available_since[i] == 0xff and cell_width[i] != 0xff:
			accept = False
			for low, high in reserved_double_width_ranges:
				if i >= low and i <= high:
					accept = True
					break
			if not accept:
				print >>sys.stderr, "Cell width data available (%d), but no age for %04X!" % (cell_width[i], i)
				sys.exit(1)

	# Default is width 1 (value 2 << 6) and version 0x3f. I.e. 0xbf or 191
	statrie = os.popen("statrie -p -r0x110000 -H'#include \"t3window/window_api.h\"' -e'T3_WINDOW_LOCAL extern' " +
		"-d generated -D191 -f chardata -n get_chardata t3_window_chardata", "w")

	for i in xrange(0, 0x110000):
		if available_since[i] == 0xff and cell_width[i] == 0xff:
			continue
		width = cell_width[i]
		if width == 0xff:
			width = 1
		# Shift range from -1-2 to 0-3.
		width += 1
		statrie.write('{0} {1}\n'.format(i, (available_since[i] & 0x3f) | (width << 6)))

	statrie.close()

	versions_header = open("generated/versions.h", "w+")
	versions_header.write("/* This file has been automtically generated by {0}. DO NOT EDIT. */\n".format(os.path.basename(sys.argv[0])))
	versions_header.write("#ifndef T3_WINDOW_VERSIONS_H\n#define T3_WINDOW_VERSIONS_H\nenum {\n")
	first = True
	for version in versions:
		if not first:
			versions_header.write(",\n")
		else:
			first = False
		versions_header.write("\tT3_UNICODE_{0}".format(version.replace(".", "")))
	versions_header.write("\n};\n#endif\n")
	versions_header.close()

	has_double_width = set()
	has_zero_width = set()
	has_combining_zero_width = set()
	double_width_tests = {}
	zero_width_tests = {}
	for i in xrange(0, 0x110000):
		if cell_width[i] == 2:
			has_double_width.add(available_since[i])
			if available_since[i] != 0xff and available_since[i] not in double_width_tests:
				accept = True
				for low, high in reserved_double_width_ranges:
					if i >= low and i <= high:
						accept = False
						break
				for low, high in double_width_test_exclude_ranges:
					if i >= low and i <= high:
						accept = False
						break

				if accept:
					double_width_tests[available_since[i]] = i
		elif cell_width[i] == 0:
			has_zero_width.add(available_since[i])
			if available_since[i] != 0xff and (available_since[i] not in zero_width_tests or (
					available_since[i] not in has_combining_zero_width and i in combining_characters)):
				accept = True
				for low, high in reserved_double_width_ranges:
					if i >= low and i <= high:
						accept = False
						break
				for low, high in double_width_test_exclude_ranges:
					if i >= low and i <= high:
						accept = False
						break

				if accept:
					zero_width_tests[available_since[i]] = i
					if i in combining_characters:
						has_combining_zero_width.add(available_since[i])

	for i in xrange(1, len(versions)):
		if i in has_double_width and not i in double_width_tests:
			print >>sys.stderr, "Could not find a suitable test for double width characters for version %s" % versions[version]
			sys.exit(1)
		if i in has_zero_width and not i in zero_width_tests:
			print >>sys.stderr, "Could not find a suitable test for zero width characters for version %s" % versions[version]
			sys.exit(1)

	tests_header = open("generated/capability_test.h", "w+")
	tests_header.write("/* This file has been automtically generated by {0}. DO NOT EDIT. */\n".format(os.path.basename(sys.argv[0])))
	sorted_zero_width_tests = sorted(zero_width_tests.iteritems())[1:]
	for version, codepoint in sorted_zero_width_tests:
		utf8_str = unichr(codepoint).encode('utf-8')
		test_str = ''.join(["\\x%02X" % ord(i) for i in utf8_str])
		tests_header.write(inspect.cleandoc("""/* [%s] U+002E FULL STOP / U+%04X %s */
			TEST("\\x2e%s",
				if (_t3_term_encoding == _T3_TERM_UTF8 && column == 1 && _t3_term_combining == T3_UNICODE_%s)
					_t3_term_combining = T3_UNICODE_%s;
			)""" % (versions[version], codepoint, name_mapping[codepoint], test_str,
				versions[version - 1].replace(".", ""), versions[version].replace(".", ""))))
		tests_header.write('\n')

	sorted_double_width_tests = sorted(double_width_tests.iteritems())[1:]
	for version, codepoint in sorted_double_width_tests:
		utf8_str = unichr(codepoint).encode('utf-8')
		test_str = ''.join(["\\x%02X" % ord(i) for i in utf8_str])
		tests_header.write(inspect.cleandoc("""/* [%s] U+%04X %s */
			TEST("%s",
				if (_t3_term_encoding == _T3_TERM_UTF8 && column == 2 && _t3_term_double_width == T3_UNICODE_%s)
					_t3_term_double_width = T3_UNICODE_%s;
			)""" % (versions[version], codepoint, name_mapping[codepoint], test_str,
				versions[version - 1].replace(".", ""), versions[version].replace(".", ""))))
		tests_header.write('\n')

	tests_header.close()

if __name__ == "__main__":
	main()
